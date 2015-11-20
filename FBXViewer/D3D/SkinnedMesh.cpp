#include "stdafx.h"
#include "SkinnedMesh.h"
#include "FBX\FbxExtractor.h"
#include "TMesh.h"
#include "Skeleton.h"
#include "Animation.h"

SkinnedMesh::SkinnedMesh( void ) :
m_pFbxExtractor(NULL),
    m_pSkeleton(NULL),
    m_pAnimation(NULL),
    m_nBone(0)
{
    D3DXMatrixIdentity(&mMatWorld); //Ĭ������£�������������任Ϊû�б任

    ZeroMemory(&boneMeshMaterial, sizeof(D3DMATERIAL9));
    boneMeshMaterial.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
    boneMeshMaterial.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
}

SkinnedMesh::~SkinnedMesh( void )
{

}


void SkinnedMesh::Destroy()
{
    for (unsigned int i=0; i<mMeshes.size(); i++)
    {
        SAFE_DELETE(mMeshes.at(i));
    }
    SAFE_DELETE(m_pSkeleton);
    SAFE_DELETE(m_pAnimation);

    SAFE_DELETE(m_pFbxExtractor);

}

void SkinnedMesh::Load( const char* fbxSrc, IDirect3DDevice9* pDevice)
{
    bool bResult = false;
    m_pFbxExtractor = new FbxExtractor(fbxSrc);
    //Mesh�б�
    mMeshes = m_pFbxExtractor->GetMeshes();
    //Ψһ�Ĺ���
    m_pSkeleton = m_pFbxExtractor->GetSkeleton();
    //Ψһ�Ķ���
    m_pAnimation = m_pFbxExtractor->GetAnimation();

    unsigned int nMeshes = mMeshes.size();
    for(unsigned int i=0; i<nMeshes; i++)
    {
        TMesh* pMesh = mMeshes.at(i);
        if (pMesh->m_bStatic)   //!!ע���SkinnedMesh::Render���߼�����һ��
        {
            DebugPrintf("Mesh %s�Ǿ�̬�ģ��Թ�\n", pMesh->mName.c_str());
            continue;
        }
        bResult = pMesh->Create(pDevice);
        if (!bResult)
        {
            DebugPrintf("����TMesh %s ʧ��\n", pMesh->mName.c_str());
        }
        else
        {
            DebugPrintf("������TMesh %s\n", pMesh->mName.c_str());
        }
    }

    SetBoneMatPtr();
    Update(mMatWorld, 0U);

    //�������������б�
    BuildBoneMesh();
}

void SkinnedMesh::SetBoneMatPtr()
{
    unsigned int nBones = m_pSkeleton->NumBones();
    DebugAssert(nBones<=MAX_BONE_COUNT, "������������50\n");
    mBoneCurrentMat.resize(nBones);
    m_nBone = nBones;
}

void SkinnedMesh::Update( D3DXMATRIX matWorld, unsigned int time )
{
    UpdateAnimation(matWorld, time);
    UpdateBoneMesh(matWorld, time);
}

void SkinnedMesh::UpdateAnimation( D3DXMATRIX matWorld, unsigned int time )
{
    SetPose(matWorld, time);
}

void SkinnedMesh::SetPose( D3DXMATRIX matWorld, unsigned int time )
{
    mMatWorld = matWorld;
    unsigned int lDuration = m_pAnimation->GetDuration();
    unsigned int currentFrame = (unsigned int)time%lDuration;
    //DebugPrintf("Frame count: %d | Duration: %d | Current animation frame: %d\n", time, lDuration, currentFrame);
    for (unsigned int i=0; i<m_nBone; i++)
    {
        Bone* pBone = m_pSkeleton->GetBone(i);
        memcpy(&pBone->matOffset, &m_pAnimation->GetFrame(i, currentFrame), sizeof(D3DXMATRIX));   //get offsetMatrix at time 'time'
    }
}

void SkinnedMesh::Render(IDirect3DDevice9* pDevice,
    const D3DXMATRIX& matWorld, const D3DXMATRIX& matView, const D3DXMATRIX& matProj, const D3DXVECTOR3& eyePoint)
{
    D3DXMATRIX matViewProj;
    D3DXMatrixMultiply(&matViewProj, &matView, &matProj);
    unsigned int nBones = m_pSkeleton->NumBones();
    //���㵱ǰ�Ĺ���������Щ�������ÿ��Mesh����һ����
    for (unsigned int i=0; i<nBones; i++)
    {
        Bone* pBone = m_pSkeleton->GetBone(i);
        D3DXMATRIX matInverse;
        D3DXMatrixInverse(&matInverse, NULL, &pBone->matBone);
        D3DXMatrixMultiply(&mBoneCurrentMat[i], &matInverse, &pBone->matOffset);
    }
    //����Constants����������Mesh
    unsigned int nMeshes = mMeshes.size();
    for (unsigned int i=0; i<nMeshes; i++)
    {
        TMesh* pMesh = mMeshes.at(i);
        if (pMesh->m_bStatic)
        {
            //DebugPrintf("Mesh %s�Ǿ�̬�ģ��Թ�Render\n", pMesh->mName.c_str());
            continue;
        }
        pMesh->SetSkinnedConstants(pDevice, matWorld, matViewProj, eyePoint, &mBoneCurrentMat[0], m_nBone);
        //pMesh->Update(pDevice, &mBoneCurrentMat[0], m_nBone);   //software skinning
        pMesh->Draw(pDevice);
    }
    RenderBoneMesh(pDevice, matWorld, matView, matProj);
}

void SkinnedMesh::BuildBoneMesh()
{
    unsigned int nBones = m_pSkeleton->NumBones();
    for (unsigned int i=0; i<nBones; i++)
    {
        Bone* pBone = m_pSkeleton->GetBone(i);
        int lParentBoneID = pBone->mParentId;
        if (lParentBoneID == -1)    //indexΪi��boneΪ������
        {
            //D3DXMATRIX& w = pBone->matBone*pBone->matOffset;
            //DebugPrintf("Root Bone \"%s\" positon at time %d : (%.3f, %.3f, %.3f)\n",
            //    pBone->mName.c_str(), time,
            //    w._41, w._42, w._43);
            continue;
        }
        Bone* pParentBone = m_pSkeleton->GetBone(lParentBoneID);
        D3DXMATRIX& w1 = pBone->matBone;
        D3DXMATRIX& w2 = pParentBone->matBone;

        //Extract translation
        D3DXVECTOR3 thisBone = D3DXVECTOR3(w1(3, 0), w1(3, 1), w1(3, 2));
        D3DXVECTOR3 ParentBone = D3DXVECTOR3(w2(3, 0), w2(3, 1), w2(3, 2));

        if(D3DXVec3Length(&(thisBone - ParentBone)) < 100.0f)
        {
            mBonePositions.push_back(SkeletonVertex(ParentBone, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f)));
            mBonePositions.push_back(SkeletonVertex(thisBone,   D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f)));
        }
    }
}

void SkinnedMesh::UpdateBoneMesh( const D3DXMATRIX& matWorld, unsigned int time )
{
    mBonePositions.resize(0);
    unsigned int nBones = m_pSkeleton->NumBones();
    for (unsigned int i=0; i<nBones; i++)
    {
        Bone* pBone = m_pSkeleton->GetBone(i);
        int lParentBoneID = pBone->mParentId;
        if (lParentBoneID == -1)    //indexΪi��boneΪ������
        {
            //D3DXMATRIX& w = pBone->matBone*pBone->matOffset;
            //DebugPrintf("Root Bone \"%s\" positon at time %d : (%.3f, %.3f, %.3f)\n",
            //    pBone->mName.c_str(), time,
            //    w._41, w._42, w._43);
            continue;
        }
        D3DXMATRIX w1, w2;
        Bone* pParentBone = m_pSkeleton->GetBone(lParentBoneID);
        w1 = pBone->matOffset;
        w2 = pParentBone->matOffset;

        //Extract translation
        D3DXVECTOR3 thisBone = D3DXVECTOR3(w1(3, 0), w1(3, 1), w1(3, 2));
        D3DXVECTOR3 ParentBone = D3DXVECTOR3(w2(3, 0), w2(3, 1), w2(3, 2));

        if(D3DXVec3Length(&(thisBone - ParentBone)) < 100.0f)
        {
            mBonePositions.push_back(SkeletonVertex(ParentBone, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f)));
            mBonePositions.push_back(SkeletonVertex(thisBone,   D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f)));
        }
        //DebugPrintf("Bone \"%s\" positon at time %d : (%.3f, %.3f, %.3f)\n",
        //    pBone->mName.c_str(), time,
        //    thisBone.x, thisBone.y, thisBone.z);
    }
    //DebugPrintf("---------------------------\n");
}

void SkinnedMesh::RenderBoneMesh(IDirect3DDevice9* pDevice,
    const D3DXMATRIX& matWorld, const D3DXMATRIX& matView, const D3DXMATRIX& matProj)
{
    HRESULT hr = S_FALSE;
    unsigned int nBonePositions = mBonePositions.size();

    V(pDevice->SetTransform(D3DTS_WORLD, &mMatWorld));
    V(pDevice->SetTransform(D3DTS_VIEW, &matView));
    V(pDevice->SetTransform(D3DTS_PROJECTION, &matProj));
    V(pDevice->SetRenderState(D3DRS_LIGHTING, FALSE));
    V(pDevice->SetRenderState(D3DRS_ZENABLE, FALSE));
    V(pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE));
    V(pDevice->SetMaterial(&boneMeshMaterial));
    V(pDevice->SetTexture(0,0));
    V(pDevice->DrawPrimitiveUP(D3DPT_LINELIST, nBonePositions/2, &mBonePositions[0], sizeof(SkeletonVertex)));
    V(pDevice->SetFVF(NULL));
    V(pDevice->SetRenderState(D3DRS_ZENABLE, TRUE));
    V(pDevice->SetRenderState(D3DRS_LIGHTING, TRUE));
}




