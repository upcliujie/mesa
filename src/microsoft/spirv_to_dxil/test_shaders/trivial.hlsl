struct PSInput {
	float4 pos : SV_Position;
};

PSInput VSMain(float4 position : POSITION)
{
        PSInput output;
        output.pos = position;
	return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(1.0, 0.0, 1.0, 1.0);
}
