
//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_Input
{
    float4 pos : POS;
    float4 color : COL;
};

struct VS_Output
{
    float4 position : SV_POSITION;
    float4 color : COL;
};

VS_Output VS_main(VS_Input input)
{
    VS_Output output = (VS_Output) 0;
    output.position = mul(input.pos, World);
    output.position = mul(output.position, View);
    output.position = mul(output.position, Projection);
    output.color = input.color;
    return output;
}

float4 PS_main(VS_Output input) : SV_TARGET
{    
    return input.color;
}
