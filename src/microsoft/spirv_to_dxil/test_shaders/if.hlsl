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
  float output = 0.0;
  for (int i = 0; i < int(input.pos.x); i++) {
    output += 1.0;
  }
  return float4(output, 0.0, 1.0, 1.0);
}
