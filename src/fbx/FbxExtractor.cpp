﻿#include "stdafx.h"
#include "FbxExtractor.h"
#include "FBXCommon.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>

#include "..\graphics\Mesh.h"
#include "..\graphics\Skeleton.h"
#include "..\graphics\Animation.h"

#pragma region Dump开关
#define DumpMeshTransformation
#define DumpBones 1
#define DumpAnimation 0
#define DumpRootNodeTransformations 0
#define DumpNodeGeometryTransformation 1
#pragma endregion

#define USE_EvaluateGlobalTransform true
// Identity D3D Matrix
static D3DXMATRIX IdentityMatrix;

#define INVALID_INDEX 0
struct BoneIndexWeight
{
    int Index;
    double Weight;
    BoneIndexWeight():Index(INVALID_INDEX),Weight(0.0){}
    BoneIndexWeight(int index, double weight):Index(index),Weight(weight){}
};

FbxExtractor::FbxExtractor() : lSdkManager(NULL), fbxScene(NULL), m_pSkeleton(NULL), m_pAnimation(NULL)
{
    bool bResult = false;

    m_pSkeleton = new Skeleton;
    m_pAnimation = new Animation;

    D3DXMatrixIdentity(&IdentityMatrix);

    // init FBX SDK
    InitializeSdkObjects(lSdkManager, fbxScene);
}

void FbxExtractor::DoExtract(const char * src)
{
    bool bResult = LoadScene(lSdkManager, fbxScene, src);
    DebugAssert(true == bResult && NULL != fbxScene, "LoadScene failed\n");

    ExtractHierarchy();

    for each (auto pair in this->FbxMeshMap)
    {
        Mesh* mesh = pair.first;
        FbxMesh* fbxMesh = pair.second;
        mesh->m_bStatic = !ExtractWeight(mesh, fbxMesh);
        SplitVertexForUV(mesh);//在Mesh中复制顶点以使得UV和顶点位置一一对应
        //pMesh->Dump(1);
        /* testing max script:
        getTVFace $st_001 1
        [13,27,28]
        getTVert $st_001 13
        [0.437086,0.733663,0]
        getTVert $st_001 27
        [0.335443,0.953107,0]
        getTVert $st_001 28
        [0.416814,0.953107,0]

        Dump output:
        Triangle 0
        ....
        TexCood
        (0.437, 0.734)
        (0.335, 0.953)
        (0.417, 0.953)

        一致
        */
    }

    ExtractAnimation();
}

FbxExtractor::~FbxExtractor(void)
{

}

namespace{
bool compareWeight(BoneIndexWeight x, BoneIndexWeight y)
{
    if (x.Weight - y.Weight > 0.00001)
    {
        return true;
    }
    return false;
}

//向MeshWeight中添加索引和权重
//添加后MeshWeight中的权重从0到3是从大到小排列的
void AddWeight(std::vector<BoneIndexWeight>& MeshWeight, BoneIndexWeight& input)
{
    //找出MeshWeight的4个weight和input的weight这5个weight中的最大4个weight
    //这4个weight即新的MeshWeight
    MeshWeight.push_back(input);
    std::sort(MeshWeight.begin(), MeshWeight.end(), compareWeight);
    MeshWeight.resize(4);
}
}

//获取和某mesh中各顶点相关联的骨骼索引和相应的权重
bool FbxExtractor::ExtractWeight(Mesh* pMesh, FbxMesh* pFbxMesh)
{
    // 一个Mesh只能有1个Deformer，并且必须是Skin deformer
    int lSkinCount = pFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
    if (0 == lSkinCount)
    {
        DebugPrintf("该Mesh不含Skin Deformer\n");
        return false;
    }

    DebugAssert(1 == lSkinCount, "该Mesh有多个Skin Deformer\n");

    const unsigned int nVertices = pMesh->nVertices;
    //MeshWeight[i]: Mesh的索引为i的顶点的4个（骨骼ID，权重值）对
    std::vector<std::vector<BoneIndexWeight> > MeshWeight;
    MeshWeight.resize(nVertices);
    for (unsigned int i=0; i<MeshWeight.size(); i++)
    {
        MeshWeight[i].resize(4);
    }

    FbxSkin* pFbxSkinDeformer = (FbxSkin *)pFbxMesh->GetDeformer(0, FbxDeformer::eSkin);
    int lClusterCount = pFbxSkinDeformer->GetClusterCount();

    //提取所有顶点的权重
    /*
        FBX中是按照骨骼来存储权重信息的，所以需要遍历完所有骨骼才能获取到所有顶点的权重信息
    */
    for (unsigned int j=0; j!=lClusterCount; ++j)   //j:index of clusters
    {
        FbxCluster* lCluster = pFbxSkinDeformer->GetCluster(j);
        FbxNode* pLinkNode = lCluster->GetLink();
        int boneIndex = 0;
        DebugAssert(pLinkNode != NULL, "与Skin Deformer %s 关联的骨骼不存在，可能是fbx文件错误\n",
            lCluster->GetName());
        /*
        之前在FbxExtractor::ExtractBone中提取的骨骼的name即其FbxNode的name，
        所以此处依据FbxNode的name来查找相应的骨骼
        */
        std::string BoneName = pLinkNode->GetName();
        boneIndex = m_pSkeleton->GetBoneIndex(BoneName);
        DebugAssert(boneIndex !=-1, "名为%s的骨骼不存在\n", BoneName.c_str());
        //与该Cluster关联的顶点的索引
        int lIndexCount = lCluster->GetControlPointIndicesCount();  //数量
        int* lIndices = lCluster->GetControlPointIndices();         //索引值
        //与该Cluster关联的顶点的权重，与lIndices一一对应
        double* lWeights = lCluster->GetControlPointWeights();
        //仅保存最大的3个权重和相关的骨骼的索引
        for(int k = 0; k < lIndexCount; k++)
        {
            int vertexIndex = lIndices[k];
            double weight = lWeights[k];
			//weight can be zero
            BoneIndexWeight biw(boneIndex, weight);
            AddWeight(MeshWeight[vertexIndex], biw);
        }
    }
    //骨骼遍历完成，获取骨骼索引和权重
    std::vector<D3DXVECTOR4>& BoneIndices = pMesh->BoneIndices;
    std::vector<D3DXVECTOR3>& BoneWeights = pMesh->BoneWeights;
    BoneIndices.resize(nVertices);
    BoneWeights.resize(nVertices);
    for (unsigned int i=0; i<nVertices; i++)
    {
        std::vector<BoneIndexWeight>& biw = MeshWeight.at(i);
        //index
        BoneIndices[i].x = (float)biw[0].Index;
        BoneIndices[i].y = (float)biw[1].Index;
        BoneIndices[i].z = (float)biw[2].Index;
        BoneIndices[i].w = (float)biw[3].Index;
        //weight
        BoneWeights[i].x = (float)biw[0].Weight;
        BoneWeights[i].y = (float)biw[1].Weight;
        BoneWeights[i].z = (float)biw[2].Weight;
#if 0
        DebugPrintf("权重信息：\n");
        const D3DXVECTOR3& v = pMesh->Positions.at(i);
        DebugPrintf("Vertex %d (%.3f, %.3f, %.3f)\n", i, v.x, v.y, v.z);
        DebugPrintf("0 Bone \"%s\"(%d), Weight %.3f\n", m_pSkeleton->GetBone(biw[0].Index)->mName.c_str(),
            biw[0].Index, biw[0].Weight);
        if (biw[1].Index !=-1)
        {
            DebugPrintf("1 Bone \"%s\"(%d), Weight %.3f\n", m_pSkeleton->GetBone(biw[1].Index)->mName.c_str(),
                biw[1].Index, biw[1].Weight);
        }
        if (biw[2].Index !=-1)
        {
            DebugPrintf("2 Bone \"%s\"(%d), Weight %.3f\n", m_pSkeleton->GetBone(biw[2].Index)->mName.c_str(),
                biw[2].Index, biw[2].Weight);
        }
        if (biw[3].Index !=-1)
        {
            DebugPrintf("3 Bone \"%s\"(%d), Weight %.3f\n", m_pSkeleton->GetBone(biw[3].Index)->mName.c_str(),
                biw[3].Index, 1 - biw[0].Weight - biw[1].Weight - biw[2].Weight);
        }
        DebugPrintf("---------------------------\n");
#endif
    }
    return true;
}

void FbxExtractor::ExtractHierarchy()
{
    FbxNode* rootNode = fbxScene->GetRootNode();
    DebugAssert(NULL!=rootNode, "invalid FbxScene，rootNode为NULL\n");

#if DumpRootNodeTransformations
    {
        DebugPrintf("Root Node <%s> Geometry Transformation:\n", rootNode->GetName());
        FbxAMatrix mat = GetGeometryTransformation(rootNode);
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
        DebugPrintf("---------------------------\n");
    }

    {
        DebugPrintf("Root Node <%s> Global Transformation:\n", rootNode->GetName());
        FbxAMatrix mat = rootNode->EvaluateGlobalTransform();
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
        DebugPrintf("---------------------------\n");
    }

    {
        DebugPrintf("Root Node <%s> Local Transformation:\n", rootNode->GetName());
        FbxAMatrix mat = rootNode->EvaluateLocalTransform();
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
        DebugPrintf("---------------------------\n");
    }
#endif

    const unsigned int childCount = rootNode->GetChildCount();
    for(unsigned int i = 0; i < childCount; i++)
    {
        FbxNode* pChildNode = rootNode->GetChild(i);
        ExtractNode(pChildNode, -1);
    }
}

void FbxExtractor::ExtractNode( FbxNode* pNode, int parentID )
{
#if DumpNodeGeometryTransformation
    DebugPrintf("Node <%s> Geometry Transformation: ", pNode->GetName());
    FbxAMatrix matGeo = GetGeometryTransformation(pNode);
    FbxVector4 T = matGeo.GetT();
    DebugPrintf("    T(%.3f, %.3f, %.3f)\t", T[0], T[1], T[2]);
    FbxVector4 R = matGeo.GetR();
    DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
#endif

    FbxNodeAttribute* pNodeAttribute = pNode->GetNodeAttribute();
    if (pNodeAttribute && pNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        FbxMesh* pFbxMesh = (FbxMesh*)pNodeAttribute;
        Mesh* pMesh = ExtractStaticMesh(pFbxMesh);
        Meshes.push_back(pMesh);
        this->FbxMeshMap[pMesh] = pFbxMesh;
    }
    else
    {
        Bone* bone = ExtractBone(pNode, parentID);//a non-FbxMesh node is a bone.
        const unsigned int childCount = pNode->GetChildCount();
        for(unsigned int i = 0; i < childCount; i++)
        {
            ExtractNode(pNode->GetChild(i), bone->mBoneId);
        }
    }
}

bool FbxExtractor::SplitVertexForUV(Mesh* pMesh)
{
    //注意复制顶点时将Postion Normal Tangent Binormal BoneIndices BoneWeights也一同复制

    std::vector<WORD>& IndexBuf = pMesh->IndexBuf;
    std::vector<D3DXVECTOR3>& Positions = pMesh->Positions;

    //Per Triangle Vertex
    std::vector<D3DXVECTOR2>& UVs = pMesh->UVs;
    //Per Vertex Position
    std::vector<D3DXVECTOR3>& Normals = pMesh->Normals;
    std::vector<D3DXVECTOR3>& Tangents = pMesh->Tangents;
    std::vector<D3DXVECTOR3>& Binormals = pMesh->Binormals;
    std::vector<D3DXVECTOR4>& BoneIndices = pMesh->BoneIndices;
    std::vector<D3DXVECTOR3>& BoneWeights = pMesh->BoneWeights;
    std::vector<D3DXVECTOR2> UVsPerPosition(pMesh->nVertices, D3DXVECTOR2(0.0f, 0.0f));

    //获取每个顶点位置的第1个UV值
    std::vector<bool> indexUsed(pMesh->nVertices, false);
    for (unsigned int i=0; i<pMesh->nFaces; ++i)
    {
        for (int j=0; j<3; ++j)
        {
            const WORD& currentIndex = IndexBuf[i*3+j];
            if (indexUsed.at(currentIndex) == false)
            {
                const D3DXVECTOR2& currentUV = UVs.at(i*3+j);
                UVsPerPosition.at(currentIndex) = currentUV;
                indexUsed.at(currentIndex)=true;
            }
        }
    }

    //按照UV“分裂”顶点
    //将同一个Position处的（index相同的）UV值，复制到新顶点的信息中
	for (unsigned int i = 0; i<pMesh->nFaces; ++i)
    {
        for (int j=0; j<3; ++j)
        {
            const D3DXVECTOR2& currentUV = UVs.at(i*3+j);
            WORD& currentIndex = IndexBuf[i*3+j];
            if (currentUV != UVsPerPosition.at(currentIndex))
            {
                const D3DXVECTOR3& currentPosition = Positions.at(currentIndex);
                const D3DXVECTOR3& currentNormal = Normals.at(currentIndex);
                const D3DXVECTOR3& currentTangent = Tangents.at(currentIndex);
                const D3DXVECTOR3& currentBinormal = Binormals.at(currentIndex);


                //复制顶点Positon/UV/Normal/Tangent/Binormal / BoneIndices/BoneWeights
                Positions.push_back(currentPosition);
                UVsPerPosition.push_back(currentUV);
                Normals.push_back(currentNormal);
                Tangents.push_back(currentTangent);
                Binormals.push_back(currentBinormal);

				if (!pMesh->m_bStatic)
				{
					const D3DXVECTOR4& currentBoneIndices = BoneIndices.at(currentIndex);
					const D3DXVECTOR3& currentBoneWeights = BoneWeights.at(currentIndex);
					BoneIndices.push_back(currentBoneIndices);
					BoneWeights.push_back(currentBoneWeights);
				}

                //修改索引为复制的新顶点的位置
                currentIndex = Positions.size()-1;

            }
        }
    }

    //替换旧的按照三角形顶点保存的UV数据
    UVs = UVsPerPosition;

    //调整TMesh::nVertex
    pMesh->nVertices = Positions.size();

    return true;
}

//从FbxMesh中提取静态Mesh信息
Mesh* FbxExtractor::ExtractStaticMesh(FbxMesh* lMesh)
{
    bool bResult = false;
    DebugAssert(lMesh->IsTriangleMesh(), "该mesh不是全部由三角形构成的\n");

    Mesh* pMesh = new Mesh;   //根据需求，会在 渲染结束 / 保存到文件后 时被释放
    FbxNode* pNode = lMesh->GetNode();

#if 0
    //FbxMesh::SplitPoints会将顶点数量增加到和索引一样多，其他效果未知
    bResult = lMesh->SplitPoints();
    DebugAssert(bResult, "SplitPoints 失败\n");
#endif

    pMesh->mName = pNode->GetName();
    FbxAMatrix matLocal = pNode->EvaluateLocalTransform();

#ifdef DumpMeshTransformation
    {
        DebugPrintf("FbxMesh <%s> Pivot Transformation Info:\n", pMesh->mName.c_str());
        FbxAMatrix mat;
        lMesh->GetPivot(mat);
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
    }
    {
        DebugPrintf("Node of FbxMesh <%s> Geometry Transformation Info:\n",pMesh->mName.c_str());
        FbxAMatrix mat = GetGeometryTransformation(pNode);
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
    }

    {
        DebugPrintf("Node of FbxMesh <%s> Global Transformation Info:\n",pMesh->mName.c_str());
        FbxAMatrix mat = pNode->EvaluateGlobalTransform();
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
    }

    {
        DebugPrintf("Node of FbxMesh <%s> Local Transformation Info:\n",pMesh->mName.c_str());
        FbxAMatrix mat = pNode->EvaluateLocalTransform();
        FbxVector4 T = mat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
        FbxVector4 R = mat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
        FbxVector4 S = mat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
    }
#endif

    int i, j, lPolygonCount = lMesh->GetPolygonCount();
    pMesh->nFaces = lPolygonCount;
    int controlPointCount = lMesh->GetControlPointsCount();
    pMesh->nVertices = controlPointCount;

    std::vector<WORD>& IndexBuf = pMesh->IndexBuf;
    std::vector<D3DXVECTOR3>& Positions = pMesh->Positions;
    std::vector<D3DXVECTOR2>& UVs = pMesh->UVs;
    std::vector<D3DXVECTOR3>& Normals = pMesh->Normals;
    std::vector<D3DXVECTOR3>& Tangents = pMesh->Tangents;
    std::vector<D3DXVECTOR3>& Binormals = pMesh->Binormals;
    IndexBuf.resize(0);
    Positions.resize(controlPointCount, D3DXVECTOR3(0.0f, 0.0f, 0.0f));

    //其他数据的数量和索引的数量一样
    UVs.resize(pMesh->nFaces*3, D3DXVECTOR2(0.0f, 0.0f));
    Normals.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    Tangents.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    Binormals.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));

    //获取顶点位置
    FbxVector4* lControlPoints = lMesh->GetControlPoints();
	for (i = 0; i<pMesh->nVertices; i++)
    {
        FbxVector4 point = lControlPoints[i];
        point = matLocal.MultT(point);
        Positions.at(i).x = (float)point[0];
		Positions.at(i).y = (float)point[1];
		Positions.at(i).z = -(float)point[2];
    }

    //获取索引值
    for (i = 0; i < lPolygonCount; i++)
    {
        //polygon必须是三角形
        DebugAssert(3 == lMesh->GetPolygonSize(i), "第%d个多边形不是三角形\n", i);
        for (j = 0; j < 3; j++)
        {
            int lControlPointIndex = lMesh->GetPolygonVertex(i, j); //第i个三角形的第j个顶点的索引
            IndexBuf.push_back(lControlPointIndex);
        }
    }
    DebugAssert(pMesh->nFaces*3==IndexBuf.size(), "三角形数量和索引数量不一致\n");

    //获取UV/Normal/Tangent/Binormal数据（按照三角形顶点来保存）
    //注意：之后将修改Normal/Tangent/Binormal数据为按照顶点来保存
    int vertexId = 0;
    for (i = 0; i < lPolygonCount; i++)
    {
        DebugAssert(3 == lMesh->GetPolygonSize(i), "第%d个多边形不是三角形\n", i);
        int l = 0;
        for (j = 0; j < 3; j++)
        {
            int lControlPointIndex = lMesh->GetPolygonVertex(i, j);
            // 1. texture coordinates / UV
#pragma region UV
            for (l = 0; l < lMesh->GetElementUVCount(); ++l)
            {
                FbxGeometryElementUV* leUV = lMesh->GetElementUV( l);

                FbxGeometryElement::EMappingMode mappingMode = leUV->GetMappingMode();
                switch (mappingMode)
                {
                case FbxGeometryElement::eByPolygonVertex:
                    {
                        int lTextureUVIndex = lMesh->GetTextureUVIndex(i, j);
                        switch (leUV->GetReferenceMode())
                        {
                        case FbxGeometryElement::eDirect:
                        case FbxGeometryElement::eIndexToDirect:
                            {
                                {
                                    FbxVector2 v = leUV->GetDirectArray().GetAt(lTextureUVIndex);
                                    /*
                                        经实际渲染发现V坐标被倒置了，所以这里将V重新赋值为1-V
                                        参考：http://forums.autodesk.com/t5/fbx-sdk/uvw-coordinates-are-importing-upside-down/m-p/4179680#M3489
                                        “ I just do 1- y to get the correct orientation for rendering. ”
                                    */
                                    UVs.at(vertexId) = D3DXVECTOR2((float)v[0],1.0f-(float)v[1]);
                                }
                            }
                            break;
                        default:
                            DebugAssert(false, "reference mode not supported");
                            break;
                        }
                    }
                    break;
                //不支持下面的几种Map mode
                case FbxGeometryElement::eByPolygon:
                case FbxGeometryElement::eAllSame:
                case FbxGeometryElement::eNone:
                case FbxGeometryElement::eByControlPoint:
                    DebugAssert(false, "Unsupported mapping mode\n");
                default:
                    break;
                }
            }
#pragma endregion

            // 2. normal
#pragma region normal
            for( l = 0; l < lMesh->GetElementNormalCount(); ++l)
            {
                FbxGeometryElementNormal* leNormal = lMesh->GetElementNormal(l);
                if(leNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    switch (leNormal->GetReferenceMode())
                    {
                    case FbxGeometryElement::eDirect:
                        {
                            FbxVector4 v = leNormal->GetDirectArray().GetAt(vertexId);
                            D3DXVECTOR3 normal((float)v[0], (float)v[1], (float)v[2]);
                            Normals.at(vertexId) = normal;
                        }
                        break;
                    case FbxGeometryElement::eIndexToDirect:
                        {
                            int id = leNormal->GetIndexArray().GetAt(vertexId);
                            {
                                FbxVector4 v = leNormal->GetDirectArray().GetAt(id);
                                D3DXVECTOR3 normal((float)v[0], (float)v[1], (float)v[2]);
                                Normals.at(vertexId) = normal;
                            }
                        }
                        break;
                    default:
                        DebugAssert(false, "reference mode not supported\n");
                        break;
                    }
                }

            }
#pragma endregion

            // 3. tangent
#pragma region tangent
            for( l = 0; l < lMesh->GetElementTangentCount(); ++l)
            {
                FbxGeometryElementTangent* leTangent = lMesh->GetElementTangent(l);

                if(leTangent->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    switch (leTangent->GetReferenceMode())
                    {
                    case FbxGeometryElement::eDirect:
                        {
                            FbxVector4 v = leTangent->GetDirectArray().GetAt(vertexId);
                            D3DXVECTOR3 tangent((float)v[0], (float)v[1], (float)v[2]);
                            Tangents.at(vertexId) = tangent;
                            break;
                        }
                    case FbxGeometryElement::eIndexToDirect:
                        {
                            int id = leTangent->GetIndexArray().GetAt(vertexId);
                            FbxVector4 v = leTangent->GetDirectArray().GetAt(id);
                            D3DXVECTOR3 tangent((float)v[0], (float)v[1], (float)v[2]);
                            Tangents.at(vertexId) = tangent;
                        }
                        break;
                    default:
                        DebugAssert(false, "Reference mode not supported\n");
                        break;
                    }
                }

            }
#pragma endregion

            // 4. binormal
#pragma region binormal
            for( l = 0; l < lMesh->GetElementBinormalCount(); ++l)
            {
                FbxGeometryElementBinormal* leBinormal = lMesh->GetElementBinormal( l);

                if(leBinormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    switch (leBinormal->GetReferenceMode())
                    {
                    case FbxGeometryElement::eDirect:
                        {
                            FbxVector4 v = leBinormal->GetDirectArray().GetAt(vertexId);
                            D3DXVECTOR3 binormal((float)v[0], (float)v[1], (float)v[2]);
                            Binormals.at(vertexId) = binormal;
                            break;
                        }

                    case FbxGeometryElement::eIndexToDirect:
                        {
                            int id = leBinormal->GetIndexArray().GetAt(vertexId);
                            FbxVector4 v = leBinormal->GetDirectArray().GetAt(id);
                            D3DXVECTOR3 binormal((float)v[0], (float)v[1], (float)v[2]);
                            Binormals.at(vertexId) = binormal;
                        }
                        break;
                    default:
                        DebugAssert(false, "Reference mode not supported\n");
                        break;
                    }
                }
            }
#pragma endregion
            vertexId++; //vertexId == i*3+j
        } // for polygonSize
    } // for polygonCount

    //按照顶点分裂Normal/Tangent/Binormal
    std::vector<D3DXVECTOR3> NormalsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    std::vector<D3DXVECTOR3> TangentsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    std::vector<D3DXVECTOR3> BinormalsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));

    std::vector<bool> indexUsed(pMesh->nVertices, false);
    for (i=0; i<pMesh->nFaces; ++i)
    {
        for (j=0; j<3; ++j)
        {
            unsigned index = i*3+j;
            WORD& currentIndex = IndexBuf[index];
            if (indexUsed.at(currentIndex) == false)
            {
                const D3DXVECTOR3& currentNormal = Normals.at(index);
                const D3DXVECTOR3& currentTangent = Tangents.at(index);
                const D3DXVECTOR3& currentBinormal = Binormals.at(index);

                NormalsPerPosition.at(currentIndex) = currentNormal;
                TangentsPerPosition.at(currentIndex) = currentTangent;
                BinormalsPerPosition.at(currentIndex) = currentBinormal;

                indexUsed.at(currentIndex)=true;
            }
        }
    }
    //替换旧的按照三角形顶点保存的Normal/Tangent/Binormal数据
    Normals = NormalsPerPosition;
    Tangents = TangentsPerPosition;
    Binormals = BinormalsPerPosition;

    //提取并保存*基础*纹理贴图的文件地址，仅从层0取数据
    FbxLayerElementMaterial* pFbxMaterial = lMesh->GetLayer(0)->GetMaterials();
    if (NULL == pFbxMaterial)
    {
        DebugPrintf("该Mesh没有关联的Material\n");
    }
    else
    {
        FbxLayerElementMaterial::EMappingMode lMappingMode = pFbxMaterial->GetMappingMode();
        DebugAssert(lMappingMode==FbxLayerElementMaterial::eAllSame, "该Mesh和两个以上的Material关联\n");
        FbxSurfaceMaterial* pFbxSurfaceMaterial = pNode->GetMaterial(0);
        FbxProperty lFbxProperty = pFbxSurfaceMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[0]);
        FbxTexture* lTexture = lFbxProperty.GetSrcObject<FbxTexture>(0);
#if EXTRACT_TEXTURE
        if(lTexture)
        {
            DebugPrintf("    Textures for %s\n", lFbxProperty.GetName());
            FbxFileTexture *lFileTexture = FbxCast<FbxFileTexture>(lTexture);
            DebugAssert(lFileTexture!=NULL, "不是文件类型的Texture\n");
            DebugPrintf("    Name: \"%s\"\n", (char*)lTexture->GetName());
            pMesh->material.DiffuseMap = lFileTexture->GetFileName();
            DebugPrintf("    File Name: \"%s\"\n", lFileTexture->GetFileName());
        }
#endif
    }

    return pMesh;

}

//返回提取出的骨骼的ID
Bone* FbxExtractor::ExtractBone(FbxNode* pNode, int parentID)
{
    const char* nodeName = pNode->GetName();
    //check if the bone exists
    for (unsigned int i = 0; i < m_pSkeleton->NumBones(); ++i)
    {
        DebugAssert(m_pSkeleton->GetBone(i)->mName != nodeName, "Bone<%s> already exists.", nodeName)
    }

    Bone* pBone = new Bone();
    pBone->mBoneId = m_pSkeleton->NumBones();  //!!Bone的ID即其在m_pSkeleton的索引值
    pBone->mParentId = parentID;
    pBone->mName = nodeName;
    FbxAMatrix matBone = pNode->EvaluateGlobalTransform();
    matBone.SetS(FbxVector4(1, 1, 1, 1));//force to 1,1,1. FIXME
    pBone->matBone = FbxAMatrix_to_D3DXMATRIX(matBone);//!! Get bind pose matrix

    m_pSkeleton->mBones.push_back(pBone);
    this->FbxNodeMap[pBone] = pNode;
    if (!pBone->IsRoot())
    {
        // add its ID to the child ID list of parent bone
        m_pSkeleton->GetBone(parentID)->mChildIDs.push_back(pBone->mBoneId);
    }

    return pBone;
}

namespace
{
    struct KeyFrame
    {
        int Time;
        FbxVector4 T;
        FbxVector4 R;   //R is in euler angle
    };
}

void FbxExtractor::ExtractCurve(Bone* bone, FbxAnimLayer* pAnimLayer)
{
    bool bResult = false;
    std::vector<KeyFrame> keyFrames;
    unsigned int boneIndex = bone->mBoneId;
    FbxNode* pNode = this->FbxNodeMap[bone];
    FbxAnimCurve* pCurve = NULL;

#if USE_CURVE
    //Collect all animation curves
    std::vector<FbxAnimCurve*> curves =
    {
        pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
        pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
        pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
        pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
        pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
        pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
    };
    //Get max frame time
    int maxFrameTime = 0;
    for each (FbxAnimCurve* curve in curves)
    {
        if (nullptr == curve)
        {
            continue;
        }
        int keyCount = curve->KeyGetCount();
        FbxAnimCurveKey lastKey = curve->KeyGet(keyCount - 1);
        FbxTime keyTime = lastKey.GetTime();
        char lTimeString[256];
        keyTime.GetTimeString(lTimeString, FbxUShort(256));
        int time = atoi(lTimeString);
        maxFrameTime = std::max<int>(maxFrameTime, time);
    }
    keyFrames.resize(maxFrameTime);

    //Sample each frame from time moment 0 to maxFrameTime.
    FbxAnimCurve* curveTX = curves[0];
    FbxAnimCurve* curveTY = curves[1];
    FbxAnimCurve* curveTZ = curves[2];
    FbxAnimCurve* curveRX = curves[3];
    FbxAnimCurve* curveRY = curves[4];
    FbxAnimCurve* curveRZ = curves[5];
    for (int time = 0; time < maxFrameTime; time++)
    {
        float tx = 0; if (curveTX != nullptr) curveTX->Evaluate(time);
        float ty = 0; if (curveTY != nullptr) curveTY->Evaluate(time);
        float tz = 0; if (curveTZ != nullptr) curveTZ->Evaluate(time);
        float rx = 0; if (curveRX != nullptr) curveRX->Evaluate(time);
        float ry = 0; if (curveRY != nullptr) curveRY->Evaluate(time);
        float rz = 0; if (curveRZ != nullptr) curveRZ->Evaluate(time);
        FbxVector4 translation = FbxVector4(tx, ty, tz);
        FbxVector4 rotation = FbxVector4(ry, ry, rz);
        FbxAMatrix mat = FbxAMatrix(translation, rotation, FbxVector4(1, 1, 1));
        D3DXMATRIX d3dMat = FbxAMatrix_to_D3DXMATRIX(mat);//local transformation of this bone at this frame
        m_pAnimation->AddFrame(bone->mBoneId, bone->mName.c_str(), time, d3dMat);
    }

#elif USE_EvaluateGlobalTransform
    //获取动画帧数和每帧时间间隔
    unsigned int frameCount = 0;
    FbxTimeSpan interval;   //每帧时间间隔
    bResult = pNode->GetAnimationInterval(interval);
    if (!bResult)
    {
        DebugPrintf("Node<%s> isn't animated.\n", pNode->GetName());
        return;
    }
    FbxTime start = interval.GetStart();
    FbxTime end = interval.GetStop();
    FbxVector4 v;
    char lTimeString[256];

    FbxLongLong longstart = start.GetFrameCount();
    FbxLongLong longend = end.GetFrameCount();
    frameCount = int(longend - longstart) + 1;
    if (frameCount > 50) {
        frameCount = 50;
    }
	for (unsigned int i = 0; i < frameCount; i++)
    {
        FbxTime t; t.SetFrame(i);
        FbxAMatrix matGlobal = pNode->EvaluateGlobalTransform(t);//transform of bone in world space at time t
        unsigned short currentTime = atoi(t.GetTimeString(lTimeString, FbxUShort(256)));
        D3DXMATRIX d3dMat = FbxAMatrix_to_D3DXMATRIX(matGlobal);
        m_pAnimation->AddFrame(boneIndex, pNode->GetName(), currentTime, d3dMat);
    }
#endif

}

//Only extract first FbxAnimLayer of first FbxAnimStack of the FBXScene //TODO
void FbxExtractor::ExtractAnimation()
{
    FbxAnimLayer* pTheOnlyAnimLayer = NULL;

    int numStacks = fbxScene->GetSrcObjectCount<FbxAnimStack>();
    DebugPrintf("FBXScene 共有%d个FbxAnimStack:\n", numStacks);

    for (int i=0; i<numStacks; i++)
    {
        FbxAnimStack* pAnimStack = fbxScene->GetSrcObject<FbxAnimStack>(i);
        int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
        DebugPrintf("    FbxAnimStack \"%s\"(%d) 有%d个FbxAnimLayer:\n", pAnimStack->GetName(), i, numAnimLayers);
        for (int j=0; j<numAnimLayers; j++)
        {
            FbxAnimLayer* pLayer = pAnimStack->GetMember<FbxAnimLayer>(j);
            DebugPrintf("        FbxAnimLayer \"%s\"(%d)\n", pLayer->GetName(), j);
            pTheOnlyAnimLayer = pLayer;
            goto Only;
        }
    }
    if (pTheOnlyAnimLayer == NULL)
    {
        DebugPrintf("    No Animation Layer.\n");
        return;
    }
Only:
    FbxAnimLayer* pAnimLayer = pTheOnlyAnimLayer;
    FbxAnimCurve* lAnimCurve = NULL;
    unsigned int nBones = m_pSkeleton->NumBones();
    for (unsigned int i = 0; i < nBones; i++)
    {
        Bone* bone = m_pSkeleton->GetBone(i);
        ExtractCurve(bone, pAnimLayer);
    }
}


void FbxExtractor::DumpBone(unsigned int boneID, bool printT/* = true*/, bool printR/* = true*/, bool printS/* = true*/) const
{
    const Bone* pBone = m_pSkeleton->GetBone(boneID);
    if (pBone->mParentId == -1)
    {
        DebugPrintf("Root ");
    }
    //骨骼名字
    DebugPrintf("Bone %s(%d): \n", pBone->mName.c_str(), pBone->mBoneId);
    if (pBone->mParentId != -1)
    {
        const Bone* pParentBone = m_pSkeleton->GetBone(pBone->mParentId);
        DebugPrintf("    parent: %s(%d)\n", pParentBone->mName.c_str(), pParentBone->mBoneId);
    }
    //骨骼矩阵
    const D3DXMATRIX& mat = pBone->matBone;
    FbxAMatrix fbxmat = D3DXMATRIX_to_FbxAMatrix(mat);
    if (printT)
    {
        FbxVector4 T = fbxmat.GetT();
        DebugPrintf("    T(%.3f, %.3f, %.3f)\n", T[0], T[1], T[2]);
    }
    if (printR)
    {
        FbxVector4 R = fbxmat.GetR();
        DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
    }
    if (printS)
    {
        FbxVector4 S = fbxmat.GetS();
        DebugPrintf("    S(%.3f, %.3f, %.3f)\n", S[0], S[1], S[2]);
    }
    DebugPrintf("---------------------------\n");
}

