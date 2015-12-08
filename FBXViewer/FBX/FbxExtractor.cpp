#include "stdafx.h"
#include "FbxExtractor.h"
#include "FBXCommon.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>

#include "..\D3D\TMesh.h"
#include "..\D3D\Skeleton.h"
#include "..\D3D\Animation.h"

#pragma region Dump����
#define DumpMeshTransformation
#define DumpRootNodeTransformation
//#define DumpNodeGeometryTransformation
#pragma endregion

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

FbxExtractor::FbxExtractor(const char* src) : lSdkManager(NULL), fbxScene(NULL), m_pSkeleton(NULL), m_pAnimation(NULL)
{
    bool bResult = false;

    //�������󣬻��� ��Ⱦ���� / ���浽�ļ��� ʱ���ͷ�
    //���ھ���SkinnedMesh::Destroy()���ͷ�
    m_pSkeleton = new Skeleton;
    m_pAnimation = new Animation;

    D3DXMatrixIdentity(&IdentityMatrix);

    // init FBX SDK objects
    InitializeSdkObjects(lSdkManager, fbxScene);

    // load FbxScene
    bResult = LoadScene(lSdkManager, fbxScene, src);
    DebugAssert(true==bResult && NULL!= fbxScene, "LoadScene failed\n");

    ExtractHierarchy();

    unsigned int lMeshCount = Meshes.size();
    for (unsigned int i=0; i<lMeshCount; i++)
    {
        TMesh* pMesh = Meshes.at(i);
        FbxMesh* pFbxMesh = FbxMeshes.at(i);
        // ��ȡ����������Ȩֵ����ȡ������ؾ���
        bResult = ExtractWeight(pMesh, pFbxMesh);
        if (!bResult)   //��̬Mesh������������Ϣ
        {
            pMesh->m_bStatic = true;
        }
        else
        {
            pMesh->m_bStatic = false;
        }
        //��Mesh�и��ƶ�����ʹ��UV�Ͷ���λ��һһ��Ӧ
        SplitVertexForUV(pMesh);
        //pMesh->Dump(1);
        /*
        max script:
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

        һ��
        */
    }

    ComputeSkeletonMatrix();

    ExtractAnimation();

    //Dump Bones
    //DebugPrintf("��ȡ�Ĺ�����Ϣ:\n");
    //unsigned int nBones = m_pSkeleton->NumBones();
    //for (unsigned int i=0; i<nBones; i++)
    //{
    //    DebugPrintf("%d\n", i);
    //    DumpBone(i, true, true);
    //}

    //Dump Animation
    //DebugPrintf("��ȡ�Ķ�����Ϣ:\n");
    //m_pAnimation->Dump(true, true);

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

//��MeshWeight�����������Ȩ��
//��Ӻ�MeshWeight�е�Ȩ�ش�0��3�ǴӴ�С���е�
void AddWeight(std::vector<BoneIndexWeight>& MeshWeight, BoneIndexWeight& input)
{
    //�ҳ�MeshWeight��4��weight��input��weight��5��weight�е����4��weight
    //��4��weight���µ�MeshWeight
    MeshWeight.push_back(input);
    std::sort(MeshWeight.begin(), MeshWeight.end(), compareWeight);
    MeshWeight.resize(4);
}
}
//��ȡ��ĳmesh�и�����������Ĺ�����������Ӧ��Ȩ��
bool FbxExtractor::ExtractWeight(TMesh* pMesh, FbxMesh* pFbxMesh)
{
    // һ��Meshֻ����1��Deformer�����ұ�����Skin deformer
    int lSkinCount = pFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
    if (0 == lSkinCount)
    {
        DebugPrintf("��Mesh����Skin Deformer\n");
        return false;
    }

    DebugAssert(1 == lSkinCount, "��Mesh�ж��Skin Deformer\n");

    const unsigned int nVertices = pMesh->nVertices;
    //MeshWeight[i]: Mesh������Ϊi�Ķ����4��������ID��Ȩ��ֵ����
    std::vector<std::vector<BoneIndexWeight> > MeshWeight;
    MeshWeight.resize(nVertices);
    for (unsigned int i=0; i<MeshWeight.size(); i++)
    {
        MeshWeight[i].resize(4);
    }

    FbxSkin* pFbxSkinDeformer = (FbxSkin *)pFbxMesh->GetDeformer(0, FbxDeformer::eSkin);
    int lClusterCount = pFbxSkinDeformer->GetClusterCount();

    //��ȡ���ж����Ȩ��
    /*
        FBX���ǰ��չ������洢Ȩ����Ϣ�ģ�������Ҫ���������й������ܻ�ȡ�����ж����Ȩ����Ϣ
    */
    for (unsigned int j=0; j!=lClusterCount; ++j)   //j:index of clusters
    {
        FbxCluster* lCluster = pFbxSkinDeformer->GetCluster(j);
        FbxNode* pLinkNode = lCluster->GetLink();
        int boneIndex = 0;
        DebugAssert(pLinkNode != NULL, "��Skin Deformer %s �����Ĺ��������ڣ�������fbx�ļ�����\n",
            lCluster->GetName());
        /*
        ֮ǰ��FbxExtractor::ExtractBone����ȡ�Ĺ�����name����FbxNode��name��
        ���Դ˴�����FbxNode��name��������Ӧ�Ĺ���
        */
        std::string BoneName = pLinkNode->GetName();
        boneIndex = m_pSkeleton->GetBoneIndex(BoneName);
        DebugAssert(boneIndex !=-1, "��Ϊ%s�Ĺ���������\n", BoneName.c_str());
        //���Cluster�����Ķ��������
        int lIndexCount = lCluster->GetControlPointIndicesCount();  //����
        int* lIndices = lCluster->GetControlPointIndices();         //����ֵ
        //���Cluster�����Ķ����Ȩ�أ���lIndicesһһ��Ӧ
        double* lWeights = lCluster->GetControlPointWeights();
        //����������3��Ȩ�غ���صĹ���������
        for(int k = 0; k < lIndexCount; k++)
        {
            int vertexIndex = lIndices[k];
            double weight = lWeights[k];
            //����Ա�֤��������
            DebugAssert(vertexIndex >= 0, "������������\n");
            /*
            ���裺FBX��Ȩֵ����Ϊ0.0
            */
			DebugAssert(floatGreaterThan((float)weight, 0.0f, 0.00001f) &&
				(floatEqual((float)weight, 1.0f, 0.00001f) || floatLessThan((float)weight, 1.0f, 0.00001f)),
                "Cluster %s�Ķ���%dȨ�ص�ֵ������\n", lCluster->GetName(), vertexIndex);
            BoneIndexWeight biw(boneIndex, weight);
            AddWeight(MeshWeight[vertexIndex], biw);
        }
    }
    //����������ɣ���ȡ����������Ȩ��
    std::vector<D3DXVECTOR4>& BoneIndices = pMesh->BoneIndices;
    std::vector<D3DXVECTOR3>& BoneWeights = pMesh->BoneWeights;
    BoneIndices.resize(nVertices);
    BoneWeights.resize(nVertices);
    for (unsigned int i=0; i<nVertices; i++)
    {
        std::vector<BoneIndexWeight>& biw = MeshWeight.at(i);
        /*
            ���裺���й����������Ķ�����û��Ȩ�ص�
                  ��������������������Ƕ�������͹���û�й�����˵���ļ����������⣩
        */
        DebugAssert(!(biw[0].Index==INVALID_INDEX && floatEqual((float)biw[0].Weight,0.0f, 0.0001f)),
            "���裺 �����й����������Ķ�����û��Ȩ�صġ� ���������������ļ�����������\n");
        //DebugAssert(biw[1].Weight!=0 || floatEqual(biw[1].Weight,0.0f, 0.0001f) && biw[1].Index==0, "");
        //DebugAssert(biw[2].Weight!=0 || floatEqual(biw[2].Weight,0.0f, 0.0001f) && biw[2].Index==0, "");
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
        DebugPrintf("Ȩ����Ϣ��\n");
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
    DebugAssert(NULL!=rootNode, "invalid FbxScene��rootNodeΪNULL\n");

#ifdef DumpRootNodeTransformation
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

    //ע��rootNode�������Mesh�͹�����Ϣ

    // �����ڵ���
    const unsigned int childCount = rootNode->GetChildCount();
    for(unsigned int i = 0; i < childCount; i++)
    {
        FbxNode* pChildNode = rootNode->GetChild(i);
        //���нڵ㶼��������
        ExtractNode(pChildNode, -1);
    }
}

void FbxExtractor::ExtractNode( FbxNode* pNode, int parentID )
{
#ifdef DumpNodeGeometryTransformation
    DebugPrintf("Node <%s> Geometry Transformation: ", pNode->GetName());
    FbxAMatrix matGeo = GetGeometryTransformation(pNode);
    FbxVector4 T = matGeo.GetT();
    DebugPrintf("    T(%.3f, %.3f, %.3f)\t", T[0], T[1], T[2]);
    FbxVector4 R = matGeo.GetR();
    DebugPrintf("    R(%.3f, %.3f, %.3f)\n", R[0], R[1], R[2]);
#endif

    FbxNodeAttribute* pNodeAttribute = pNode->GetNodeAttribute();
    if (pNodeAttribute
        && pNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {//�ڵ�ΪFbxMesh
        FbxMesh* pFbxMesh = (FbxMesh*)pNodeAttribute;
        FbxMeshes.push_back(pFbxMesh);
        TMesh* pMesh = ExtractStaticMesh(pFbxMesh);
        Meshes.push_back(pMesh);
    }
    else
    {
        //���з�FbxMesh�ڵ㶼�������� TODO:�Ƿ��޶����ڵ�ΪrootNode����û���ӽڵ�Ľڵ㲻��Ϊ����������Ϊ�ǹ����ڵ㣩
        int boneID = ExtractBone(pNode, parentID);
        if (boneID == -2)   //�Ѿ���ȡ���ù��������ǲ�����ܷ����������������������ͬ����
        {
            return;
        }

        // extract info of child bones
        const unsigned int childCount = pNode->GetChildCount();
        for(unsigned int i = 0; i < childCount; i++)
        {
            ExtractNode(pNode->GetChild(i), boneID);
        }
    }
}

bool FbxExtractor::SplitVertexForUV(TMesh* pMesh)
{
    //ע�⸴�ƶ���ʱ��Postion Normal Tangent Binormal BoneIndices BoneWeightsҲһͬ����

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

    //��ȡÿ������λ�õĵ�1��UVֵ
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

    //����UV�����ѡ�����
    //��ͬһ��Position���ģ�index��ͬ�ģ�UVֵ�����Ƶ��¶������Ϣ��
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

                const D3DXVECTOR4& currentBoneIndices = BoneIndices.at(currentIndex);
                const D3DXVECTOR3& currentBoneWeights = BoneWeights.at(currentIndex);

                //���ƶ���Positon/UV/Normal/Tangent/Binormal / BoneIndices/BoneWeights
                Positions.push_back(currentPosition);
                UVsPerPosition.push_back(currentUV);
                Normals.push_back(currentNormal);
                Tangents.push_back(currentTangent);
                Binormals.push_back(currentBinormal);
                BoneIndices.push_back(currentBoneIndices);
                BoneWeights.push_back(currentBoneWeights);

                //�޸�����Ϊ���Ƶ��¶����λ��
                currentIndex = Positions.size()-1;

            }
        }
    }

    //�滻�ɵİ��������ζ��㱣���UV����
    UVs = UVsPerPosition;

    //����TMesh::nVertex
    pMesh->nVertices = Positions.size();

    return true;
}

//��FbxMesh����ȡ��̬Mesh��Ϣ
TMesh* FbxExtractor::ExtractStaticMesh(FbxMesh* lMesh)
{
    bool bResult = false;
    DebugAssert(lMesh->IsTriangleMesh(), "��mesh����ȫ���������ι��ɵ�\n");

    TMesh* pMesh = new TMesh;   //�������󣬻��� ��Ⱦ���� / ���浽�ļ��� ʱ���ͷ�
    FbxNode* pNode = lMesh->GetNode();

#if 0
    //FbxMesh::SplitPoints�Ὣ�����������ӵ�������һ���࣬����Ч��δ֪
    bResult = lMesh->SplitPoints();
    if (false == bResult)
    {
        DebugPrintf("SplitPoints ʧ��\n");
    }
#endif

    pMesh->mName = pNode->GetName();

#ifdef DumpMeshTransformation
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

    //�������ݵ�����������������һ��
    UVs.resize(pMesh->nFaces*3, D3DXVECTOR2(0.0f, 0.0f));
    Normals.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    Tangents.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    Binormals.resize(pMesh->nFaces*3, D3DXVECTOR3(0.0f, 0.0f, 0.0f));

    //��ȡ����λ��
    FbxVector4* lControlPoints = lMesh->GetControlPoints();
	for (unsigned i = 0; i<pMesh->nVertices; i++)
    {
        Positions.at(i).x = (float)lControlPoints[i][0];
		Positions.at(i).y = (float)lControlPoints[i][1];
		Positions.at(i).z = (float)lControlPoints[i][2];
    }

    //��ȡ����ֵ
    for (i = 0; i < lPolygonCount; i++)
    {
        //polygon������������
        DebugAssert(3 == lMesh->GetPolygonSize(i), "��%d������β���������\n", i);
        for (j = 0; j < 3; j++)
        {
            int lControlPointIndex = lMesh->GetPolygonVertex(i, j); //��i�������εĵ�j�����������
            IndexBuf.push_back(lControlPointIndex);
        }
    }
    DebugAssert(pMesh->nFaces*3==IndexBuf.size(), "����������������������һ��\n");

    //��ȡUV/Normal/Tangent/Binormal���ݣ����������ζ��������棩
    //ע�⣺֮���޸�Normal/Tangent/Binormal����Ϊ���ն���������
    int vertexId = 0;
    for (i = 0; i < lPolygonCount; i++)
    {
        DebugAssert(3 == lMesh->GetPolygonSize(i), "��%d������β���������\n", i);
        int l = 0;
        for (j = 0; j < 3; j++)
        {
            int lControlPointIndex = lMesh->GetPolygonVertex(i, j);
            // 1. color (not used)

            // 2. texture coordinates / UV
#pragma region UV
            for (l = 0; l < lMesh->GetElementUVCount(); ++l)
            {
                FbxGeometryElementUV* leUV = lMesh->GetElementUV( l);

                switch (leUV->GetMappingMode())
                {
                default:
                    break;
                /*
                    ���裺FbxGeometryElement::eByControlPoint�ķ�֧�Ǵ������ᱻ���ʵ���
                */
                case FbxGeometryElement::eByControlPoint:
                    DebugAssert(false, "�÷�֧��Ӧ�ñ����ʣ�����ʧ��\n");
                    switch (leUV->GetReferenceMode())
                    {
                    case FbxGeometryElement::eDirect:
                        {
                            FbxVector2 v = leUV->GetDirectArray().GetAt(lControlPointIndex);
                            //UVs[lControlPointIndex] = D3DXVECTOR2((float)v[0],(float)v[1]);
                        }
                        break;
                    case FbxGeometryElement::eIndexToDirect:
                        {
                            int id = leUV->GetIndexArray().GetAt(lControlPointIndex);
                            {
                                FbxVector2 v = leUV->GetDirectArray().GetAt(id);
                                //UVs[lControlPointIndex] = D3DXVECTOR2((float)v[0],(float)v[1]);
                            }
                        }
                        break;
                    default:
                        DebugAssert(false, "reference mode not supported");
                        break;
                    }
                    break;

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
                                        ��ʵ����Ⱦ����V���걻�����ˣ��������ｫV���¸�ֵΪ1-V
                                        �ο���http://forums.autodesk.com/t5/fbx-sdk/uvw-coordinates-are-importing-upside-down/m-p/4179680#M3489
                                        �� I just do 1- y to get the correct orientation for rendering. ��
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

                case FbxGeometryElement::eByPolygon: // doesn't make much sense for UVs
                case FbxGeometryElement::eAllSame:   // doesn't make much sense for UVs
                case FbxGeometryElement::eNone:       // doesn't make much sense for UVs
                    break;
                }
            }
#pragma endregion

            // 3. normal
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
                        break; // other reference modes not shown here!
                    }
                }

            }
#pragma endregion

            // 4. tangent
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
                        DebugAssert(false, "reference mode not supported\n");
                        break; // other reference modes not shown here!
                    }
                }

            }
#pragma endregion

            // 5. binormal
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
                        DebugAssert(false, "mapping mode not supported\n");
                        break; // other reference modes not shown here!
                    }
                }
            }
#pragma endregion
            vertexId++; //vertexId == i*3+j
        } // for polygonSize
    } // for polygonCount

    //�޸�Normal/Tangent/Binormal���ݣ����ն��㱣�棩
    std::vector<D3DXVECTOR3> NormalsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    std::vector<D3DXVECTOR3> TangentsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
    std::vector<D3DXVECTOR3> BinormalsPerPosition(pMesh->nVertices, D3DXVECTOR3(0.0f, 0.0f, 0.0f));

    std::vector<bool> indexUsed(pMesh->nVertices, false);
    for (unsigned int i=0; i<pMesh->nFaces; ++i)
    {
        for (int j=0; j<3; ++j)
        {
            WORD& currentIndex = IndexBuf[i*3+j];
            if (indexUsed.at(currentIndex) == false)
            {
                const D3DXVECTOR3& currentNormal = Normals.at(i*3+j);
                const D3DXVECTOR3& currentTangent = Tangents.at(i*3+j);
                const D3DXVECTOR3& currentBinormal = Binormals.at(i*3+j);

                NormalsPerPosition.at(currentIndex) = currentNormal;
                TangentsPerPosition.at(currentIndex) = currentTangent;
                BinormalsPerPosition.at(currentIndex) = currentBinormal;

                indexUsed.at(currentIndex)=true;
            }
        }
    }
    //�滻�ɵİ��������ζ��㱣���Normal/Tangent/Binormal����
    Normals = NormalsPerPosition;
    Tangents = TangentsPerPosition;
    Binormals = BinormalsPerPosition;

    //��ȡ������������ͼ���ļ���ַ�����Ӳ�0ȡ����
    FbxLayerElementMaterial* pFbxMaterial = lMesh->GetLayer(0)->GetMaterials();
    if (NULL == pFbxMaterial)
    {
        DebugPrintf("��Meshû�й�����Material\n");
    }
    else
    {
        FbxLayerElementMaterial::EMappingMode lMappingMode = pFbxMaterial->GetMappingMode();
        DebugAssert(lMappingMode==FbxLayerElementMaterial::eAllSame, "��Mesh���������ϵ�Material����\n");
        FbxSurfaceMaterial* pFbxSurfaceMaterial = pNode->GetMaterial(0);
        FbxProperty lFbxProperty = pFbxSurfaceMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[0]);
        FbxTexture* lTexture = lFbxProperty.GetSrcObject<FbxTexture>(0);
        if(lTexture)
        {
            DebugPrintf("    Textures for %s\n", lFbxProperty.GetName());
            FbxFileTexture *lFileTexture = FbxCast<FbxFileTexture>(lTexture);
            DebugAssert(lFileTexture!=NULL, "�����ļ����͵�Texture\n");
            DebugPrintf("    Name: \"%s\"\n", (char*)lTexture->GetName());
            pMesh->material.DiffuseMap = lFileTexture->GetFileName();
            DebugPrintf("    File Name: \"%s\"\n", lFileTexture->GetFileName());
        }
    }

    return pMesh;

}

//������ȡ���Ĺ�����ID
int FbxExtractor::ExtractBone( FbxNode* pNode, int parentID )
{
    // ����������ID: parentID�����ӹ���
    Bone* pBone = new Bone();
    pBone->mBoneId = m_pSkeleton->NumBones();  //!!Bone��ID������m_pSkeleton������ֵ
    pBone->mParentId = parentID;
    pBone->mName = pNode->GetName();
    // ���ù����Ƿ��Ѵ��ڣ����������жϣ�������������
    for (unsigned int i = 0; i < m_pSkeleton->NumBones(); ++i)
    {
        if (m_pSkeleton->GetBone(i)->mName == pBone->mName)
            return -2;
    }
    // save bone to skeleton
    m_pSkeleton->mBones.push_back(pBone);
    // �洢FbxNode��֮����ComputeSkeletonMatrix�и��������ȡ����������ExtractCurve�и��������ȡ������Ϣ
    m_pSkeleton->mFbxNodes.push_back(pNode);
    // add its ID to the child ID list of parent bone
    if (parentID != -1) //���������Ǹ�����
    {
        m_pSkeleton->GetBone(parentID)->mChildIDs.push_back(pBone->mBoneId);
    }
    else    //�������Ǹ�����
    {
        m_pSkeleton->mRootBoneIds.push_back(pBone->mBoneId);
    }
    return pBone->mBoneId;
}

void FbxExtractor::ComputeSkeletonMatrix()
{
    FbxAMatrix lMatrix;
    
    unsigned int nBone = m_pSkeleton->NumBones();
    for (unsigned int i=0; i<nBone; i++)
    {
        FbxNode* pFbxNode = m_pSkeleton->mFbxNodes[i];
        FbxTime time;
        time.SetFrame(0);
        lMatrix = pFbxNode->EvaluateGlobalTransform(time); //��0֡��Ϊbindpose
        m_pSkeleton->SetBoneMatrix(i, FbxAMatrix_to_D3DXMATRIX(lMatrix));
    }
}

namespace
{
    struct TR
    {
        unsigned short Time;
        FbxVector4 T;
        FbxVector4 R;   //R����Q!
    };
}

void FbxExtractor::ExtractCurve( unsigned int boneIndex, FbxAnimLayer* pAnimLayer )
{
    bool bResult = false;
    /*
        ����pCurveTX��pCurveTY��pCurveTZ��pCurveRX��pCurveRY��pCurveRZ��lKeyCount��֡��������ͬ��
        ���������������ģ�
    */
    std::vector<TR> frameValues;    //�洢����(boneIndex)��ÿһ֡�Ķ�����Ϣ
    FbxNode* pNode = m_pSkeleton->mFbxNodes.at(boneIndex);
    FbxAnimCurve* pCurve = NULL;

    FbxAnimCurve* pCurveTX = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    if (!pCurveTX)
    {
        DebugPrintf("����%s(%d)û�а���������Ϣ\n", pNode->GetName(), boneIndex);
        return;
    }
    int lFrameCount = pCurveTX->KeyGetCount();
    frameValues.resize(lFrameCount);
    FbxAnimCurve* pCurveTY = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    int lFrameCount_ = pCurveTY->KeyGetCount();
    DebugAssert(lFrameCount == lFrameCount_, "%s(%d) error: ���費����\n", __FILE__, __LINE__);
    FbxAnimCurve* pCurveTZ = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
    lFrameCount_ = pCurveTZ->KeyGetCount();
    DebugAssert(lFrameCount == lFrameCount_, "%s(%d) error: ���費����\n", __FILE__, __LINE__);
    FbxAnimCurve* pCurveRX = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    lFrameCount_ = pCurveRX->KeyGetCount();
    DebugAssert(lFrameCount == lFrameCount_, "%s(%d) error: ���費����\n", __FILE__, __LINE__);
    FbxAnimCurve* pCurveRY = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    lFrameCount_ = pCurveRY->KeyGetCount();
    DebugAssert(lFrameCount == lFrameCount_, "%s(%d) error: ���費����\n", __FILE__, __LINE__);
    FbxAnimCurve* pCurveRZ = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
    lFrameCount_ = pCurveRZ->KeyGetCount();
    DebugAssert(lFrameCount == lFrameCount_, "%s(%d) error: ���費����\n", __FILE__, __LINE__);

#if 0
    FbxTime lKeyTime;
    char lTimeString[256];
    float v;
    unsigned short time;    //ע�⣺ δʹ��time��ʵ�ʽ�����֡�����

    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveTX->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveTX->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).T[0] = v;
        //DebugPrintf("    time %d, TX %.3f\n", i, v);
    }
    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveTY->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveTY->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).T[1] = v;
        //DebugPrintf("    time %d, TY %.3f\n", i, v);
    }
    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveTZ->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveTZ->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).T[2] = v;
        //DebugPrintf("    time %d, TZ %.3f\n", i, v);
    }

    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveRX->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveRX->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).R[0] = v;
        //DebugPrintf("    time %d, RX %.3f\n", i, v);
    }
    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveRY->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveRY->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).R[1] = v;
        //DebugPrintf("    time %d, RY %.3f\n", i, v);
    }
    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        lKeyTime = pCurveRZ->KeyGetTime(i);
        time = atoi(lKeyTime.GetTimeString(lTimeString, FbxUShort(256)));
        v = pCurveRZ->KeyGetValue(i);
        frameValues.at(i).Time = time;
        frameValues.at(i).R[2] = v;
        //DebugPrintf("    time %d, RZ %.3f\n", i, v);
    }

    //DebugPrintf("---------------------------\n");

    //����ɢ��������䵽������
    for(int i = 0; i < lFrameCount; i++)   //��ͬ��time
    {
        TR& tr = frameValues.at(i);
        FbxAMatrix mat;
        /*
            ���裺û��Scale�任
                  tr.R[3] == 1.0(��������Ĭ��Ϊ0.0������FbxAMatrix::SetTRS�Ĺ�������)
        */
        FbxVector4 S(1.0, 1.0, 1.0);
        //DebugAssert(floatEqual(tr.R[3], 1.0f, 0.00001f), "%s(%d) error: ����tr.R[3] == 1.0 ������\n", __FILE__, __LINE__);
        mat.SetTRS(tr.T, tr.R, S);
        //if (tr.Time==0)
        //{
        //    m_pSkeleton->SetBoneMatrix(boneIndex, FbxAMatrix_to_D3DXMATRIX(mat));
        //}
        m_pAnimation->AddFrame(boneIndex, pNode->GetName(), tr.Time, FbxAMatrix_to_D3DXMATRIX(mat));
    }
#else
    //��һ�ַ���
    //��ȡ����֡����ÿ֡ʱ����
    unsigned int l_nFrameCount = 0;
    FbxTimeSpan interval;   //ÿ֡ʱ����
    bResult = pNode->GetAnimationInterval(interval);
    if (!bResult)
    {
        DebugPrintf("Bone \"%s\"����������Ϣ\n", pNode->GetName());
        return;
    }
    FbxTime start = interval.GetStart();
    FbxTime end = interval.GetStop();
    FbxVector4 v;
    char lTimeString[256];

    FbxLongLong longstart = start.GetFrameCount();
    FbxLongLong longend = end.GetFrameCount();
    l_nFrameCount = int(longend - longstart) + 1;
	for (unsigned int i = 0; i < l_nFrameCount; i++)   //��ͬ��time
    {
        FbxTime t; t.SetFrame(i);
        FbxAMatrix mat = pNode->EvaluateGlobalTransform(t); //���ﲻ�ɿ���ԭ��δ֪
        unsigned short currentTime = atoi(t.GetTimeString(lTimeString, FbxUShort(256)));
        m_pAnimation->AddFrame(boneIndex, pNode->GetName(), currentTime, FbxAMatrix_to_D3DXMATRIX(mat));
    }
#endif
}

void FbxExtractor::ExtractAnimation()
{
    FbxAnimLayer* pTheOnlyAnimLayer = NULL;

    int numStacks = fbxScene->GetSrcObjectCount<FbxAnimStack>();
    DebugPrintf("FBXScene ����%d��FbxAnimStack:\n", numStacks);

    for (int i=0; i<numStacks; i++)
    {
        FbxAnimStack* pAnimStack = fbxScene->GetSrcObject<FbxAnimStack>(i);
        int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
        DebugPrintf("    FbxAnimStack \"%s\"(%d) ��%d��FbxAnimLayer:\n", pAnimStack->GetName(), i, numAnimLayers);
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
        DebugPrintf("û�а���������Ϣ\n");
        return;
    }
Only:
    FbxAnimLayer* pAnimLayer = pTheOnlyAnimLayer;
    /*
        ����FBXSceneֻ��1��FbxAnimStack
            FbxAnimStackֻ��1��FbxAnimLayer
    */
    FbxAnimCurve* lAnimCurve = NULL;
    unsigned int nBones = m_pSkeleton->NumBones();
    for (unsigned int i=0; i<nBones; i++)
    {
        Bone* pBone = m_pSkeleton->GetBone(i);
        DebugAssert(i==pBone->mBoneId, "i!=mBoneId\n");
        //DebugPrintf("Bone \"%s\"(%d):\n", pBone->mName.c_str(), i);
        ExtractCurve(i, pAnimLayer);
    }
}


void FbxExtractor::DumpBone(unsigned int boneID, bool printT, bool printR) const
{
    const Bone* pBone = m_pSkeleton->GetBone(boneID);
    if (pBone->mParentId == -1)
    {
        DebugPrintf("Root ");
    }
    //��������
    DebugPrintf("Bone %s(%d): \n", pBone->mName.c_str(), pBone->mBoneId);
    if (pBone->mParentId != -1)
    {
        const Bone* pParentBone = m_pSkeleton->GetBone(pBone->mParentId);
        DebugPrintf("    parent: %s(%d)\n", pParentBone->mName.c_str(), pParentBone->mBoneId);
    }
    //��������
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
    DebugPrintf("---------------------------\n");
}

