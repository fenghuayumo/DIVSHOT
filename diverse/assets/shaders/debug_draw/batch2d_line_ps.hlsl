
struct PsIn
{
    [[vk::location(0)]] float4 colour : COLOR0;
};
struct PsOut {
    float4 color : SV_TARGET0;
};

PsOut main(PsIn ps)
{
    PsOut ps_out;
    ps_out.color = ps.colour;

    return ps_out;
}