// HOUSE-00087 / HOUSE-00088 -- the smallest .fx that can answer both questions:
//   * does an Effect-Framework file survive fxc -> .xnb -> MojoShader -> a draw?
//   * can two techniques be selected BY NAME per draw, and does a parameter change the output?
// Deliberately shader-model 2.0, which is what XNA 4.0's Reach profile accepts.

float4x4 WorldViewProj;
float4   TintColor;

texture BaseTexture;
sampler BaseSampler = sampler_state
{
    Texture   = <BaseTexture>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = None;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

struct VSInput
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(input.Position, WorldViewProj);
    output.TexCoord = input.TexCoord;
    return output;
}

// technique Tint: the parameter alone decides the pixel, so a parameter that did not reach the
// shader is immediately visible.
float4 PixelTint(VSOutput input) : COLOR0
{
    return TintColor;
}

// technique Textured: the texture times the parameter, so selecting the wrong technique produces a
// numerically different image rather than a subtly different one.
float4 PixelTextured(VSOutput input) : COLOR0
{
    return tex2D(BaseSampler, input.TexCoord) * TintColor;
}

technique Tint
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexMain();
        PixelShader  = compile ps_2_0 PixelTint();
    }
}

technique Textured
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexMain();
        PixelShader  = compile ps_2_0 PixelTextured();
    }
}

// HOUSE-00086 / BL-03. A stock effect declares one output, so the stock-effect MRT measurement can
// only show that the renderer does not broadcast. This technique declares TWO, which is the only
// way to ask whether attachment 1 is reachable at all.
struct PSMulti
{
    float4 Target0 : COLOR0;
    float4 Target1 : COLOR1;
};

PSMulti PixelMulti(VSOutput input)
{
    PSMulti output;
    output.Target0 = TintColor;
    // deliberately a different, unmistakable value: if attachment 1 receives anything, we know
    // whether it received ITS OWN output or a copy of attachment 0's
    output.Target1 = float4(0.125, 0.875, 0.375, 1.0);
    return output;
}

technique MultiTarget
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexMain();
        PixelShader  = compile ps_2_0 PixelMulti();
    }
}
