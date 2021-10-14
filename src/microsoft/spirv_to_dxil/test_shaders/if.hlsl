struct PSInput {
  float4 pos : SV_Position;
};

PSInput VSMain(float4 position : POSITION) {
  PSInput output;
  if (position.x == 0) {
    output.pos = float4(0.0, 0.0, 0.0, 0.0);
  } else {
    output.pos = float4(010, 010, 010, 010);
  }
  return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
  return float4(1.0, 0.0, 1.0, 1.0);
}
