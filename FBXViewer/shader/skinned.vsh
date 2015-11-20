uniform extern float4x4 matWorld;
uniform extern float4x4 matViewProj;
uniform extern float4x4 matWorldInverseTranspose;

//����
float4 gAmbient;			//����������ɫ
float4 gDiffuse;			//ɢ�������ɫ
float4 gSpecular;		//���������ɫ
float gSpecularPower;	//�������ָ��

float4 gMaterialAmbient;	//���ʣ��������գ�
float4 gMaterialDiffuse;	//���ʣ�ɢ����գ�
float4 gMaterialSpecular;	//���ʣ�������գ�

float3 gEyePosition;		//Eye��λ��

float3 gPointLightPosition;	//���Դ��λ��
float3 gAttenuation012;		//˥��

float4x4 MatrixPalette[50];	//���й���������任����

struct InputVS
{
	float3 Pos:			POSITION0;
	float3 Normal:		NORMAL0;
	float2 TC:			TEXCOORD0;
	float4 BoneIndices:	BLENDINDICES;
	float3 Weights:		BLENDWEIGHT;
};

struct OutputVS
{
	float4 Pos:		POSITION0;
	float3 Normal:	NORMAL0;
	float2 TC:		TEXCOORD0;
	float4 Color:	COLOR0;
};

OutputVS main(InputVS In)
{
	OutputVS outVS = (OutputVS)0;

	//skinning ����

	float4 p = {0.0f, 0.0f, 0.0f, 1.0f};
	float3 norm = {0.0f, 0.0f, 0.0f};
	//if(In.BoneIndices[3] == -10){
		p +=    In.Weights.x*mul(float4(In.Pos.xyz,1.0f),MatrixPalette[In.BoneIndices[0]]);
		norm += In.Weights.x*mul(float4(In.Normal,1.0f), MatrixPalette[In.BoneIndices[0]]).xyz;
	
		p +=    In.Weights.y*mul(float4(In.Pos.xyz,1.0f), MatrixPalette[In.BoneIndices[1]]);
		norm += In.Weights.y*mul(float4(In.Normal,1.0f), MatrixPalette[In.BoneIndices[1]]).xyz;

		p +=    In.Weights.z*mul(float4(In.Pos.xyz,1.0f), MatrixPalette[In.BoneIndices[2]]);
		norm += In.Weights.z*mul(float4(In.Normal,1.0f), MatrixPalette[In.BoneIndices[2]]).xyz;

		float finalWeight = 1 - In.Weights.x - In.Weights.y - In.Weights.z;
		p +=    finalWeight*mul(float4(In.Pos.xyz,1.0f), MatrixPalette[In.BoneIndices[3]]);
		norm += finalWeight*mul(float4(In.Normal,1.0f), MatrixPalette[In.BoneIndices[3]]).xyz;
	//}else{p.xyz = In.Pos.xyz; norm = In.Normal;}
	p.w = 1.0f;
	outVS.Pos = p;

	outVS.Pos = mul(outVS.Pos, matWorld);
	float3 PosW = outVS.Pos.xyz;
	outVS.Pos = mul(outVS.Pos, matViewProj);

	outVS.TC = In.TC;

	//���㷨��
	norm = normalize(norm);
	float3 normalW = mul(float4(norm, 0.0f), matWorldInverseTranspose).xyz;
	normalW = normalize(normalW);
	outVS.Normal = normalW;
	//����ⷽ��
	float3 lightDir = normalize(PosW - gPointLightPosition);
	//����ⷽ��
	float3 r = reflect(lightDir, normalW);
	//��ǰ���㵽�۾��ĵ�λ����
	float3 toEye = normalize(gEyePosition - PosW);
	//��ǰ���㵽��Դ�ľ���
	float d = distance(PosW, gPointLightPosition);
	//˥��
	float Attentuation = gAttenuation012.x + gAttenuation012.y*d + gAttenuation012.z*d*d;

	//�߹�
	float t = pow(max(dot(r, toEye), 0.0f), gSpecularPower);
	float3 specular = t*(gMaterialSpecular*gSpecular).rgb;
	//ɢ���
	float s = max(dot(lightDir, normalW), 0.0f);
	float3 diffuse = s*(gMaterialDiffuse*gDiffuse).rgb;
	//������
	float3 ambient = (gMaterialAmbient*gAmbient).rgb;

	//�ܹ���
	outVS.Color.rgb = ambient + (diffuse + specular)/Attentuation;	//ֻ��ɢ���͸߹���˥����Ӱ��
	outVS.Color.a = gMaterialDiffuse.a;

	return outVS;
}
