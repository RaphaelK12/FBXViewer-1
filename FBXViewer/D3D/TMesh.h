#pragma once
#include <d3d9.h>
#include <vector>

//���㶨��
struct Vertex
{
    D3DXVECTOR3 Pos;    //0 + 12
    D3DXVECTOR3 Normal; //12 + 12
    D3DXVECTOR2 TC;     //24 + 8
    D3DXVECTOR4 BoneIndices;    //32 + 16
    D3DXVECTOR3 BoneWeights;    //48 + 12
};

class Skeleton;
class TMesh
{
private:
    TMesh(TMesh&);
    void operator = (TMesh&);

    struct Matrial
    {
        D3DXCOLOR Diffuse;
        D3DXCOLOR Ambient;
        D3DXCOLOR Specular;

        std::string DiffuseMap; //��������ͼ��������ͼ��·��
    };

    //�������
    D3DXCOLOR Ambient;      //��������ɫ
    D3DXCOLOR Diffuse;      //ɢ�����ɫ
    D3DXCOLOR Specular;     //�������ɫ
    float SpecularPower;    //�����ָ��

    D3DXVECTOR3 LightPos;       //���Դλ��
    D3DXVECTOR3 Attenuation012; //����˥��ϵ��


    D3DXVECTOR3 EyePosition;

    IDirect3DIndexBuffer9*          pIB;
    IDirect3DVertexBuffer9*         pVB;
    IDirect3DVertexDeclaration9*    pVD;
    IDirect3DTexture9*              pTexture;

    IDirect3DVertexShader9*         pVS;
    IDirect3DPixelShader9*          pPS;
    ID3DXConstantTable*             pCTVS;
    ID3DXConstantTable*             pCTPS;

public:
    bool m_bStatic; //�Ƿ�Ϊ��̬mesh��ע����̬mesh�޹�����skinning����Ϣ
    unsigned int nVertices;
    unsigned int nFaces;
    std::vector<WORD> IndexBuf;             // index

    std::vector<D3DXVECTOR3> Positions;     //positions
    std::vector<D3DXVECTOR2> UVs;           //texture coordinates (only one now)
    std::vector<D3DXVECTOR3> Normals;       //normals
    std::vector<D3DXVECTOR3> Tangents;      //tangents
    std::vector<D3DXVECTOR3> Binormals;     //binormals

    //��Ƥ������Ϣ
    std::vector<D3DXVECTOR4> BoneIndices;   //��ͷ����
    std::vector<D3DXVECTOR3> BoneWeights;   //����Ȩ�أ���4��Ȩ����1-ǰ3���õ�

    //����
    Matrial material;

    TMesh(void);
    ~TMesh(void);

    std::string mName;

    void SetSkinnedConstants( IDirect3DDevice9* pDevice,
        const D3DXMATRIX& matWorld, const D3DXMATRIX& matViewProj, const D3DXVECTOR3& eyePoint,
        const D3DXMATRIX* matBone, unsigned int nBones);

    bool Create(IDirect3DDevice9* pDevice);

    bool Update(IDirect3DDevice9* pDevice, const D3DXMATRIX* matBone, unsigned int nBones);

    void Draw(IDirect3DDevice9* pDevice);

    void Destroy();
    
    /*
        Ĭ�������TMesh��õ�ģ����������ϵ�¡�Z���ϡ�Y���ڡ�X���ҵģ���3DSMAX����ͬ��
        Convert������TMesh����ת����ʹ��ת��Ϊ������ϵ�¡�Y���ϡ�Z���ڡ�X���ҵģ���D3DĬ�ϵ���ͬ��
        ע�⣺�����ڣ�2014��9��16��10:07:01������Ծ�̬ģ�ͣ����ҽ��Զ���������������б任��
              ��ת����Normal��Tangent��Binormal��ʧȥ���壬��Ϊģ�͵Ķ������귢���˱仯��������Ҫ���¼���
              �۶�UVs��BoneIndices��BoneWeightsû��Ӱ��
    */
    enum MeshType{RIGHTHANDED_ZUP, LEFTHANDED_YUP} mMeshType;
    void Convert(MeshType targetType = LEFTHANDED_YUP);

    //Debug functions
    void Dump(unsigned int n);
};

