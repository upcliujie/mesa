struct PSInput {
  float4 pos : SV_Position;
};

PSInput VSMain(float4 position : POSITION) {
  PSInput output;
  output.pos = position;
  return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
  float output = 0.0;
  switch (input.pos.z) {
    case 0:
      output = abs(input.pos.x);
      break;
    case 1:
      output = acos(input.pos.x) + asin(input.pos.x) + atan(input.pos.x) + cos(input.pos.x) + sin(input.pos.y) + tan(input.pos.z);
      break;
    case 2:
      output = all(input.pos);
      break;
    case 3:
      output = any(input.pos);
      break;
          case 4:
      output = floor(input.pos.x) + ceil(input.pos.x);
      break;
  }
  return float4(output, 0.0, 1.0, 1.0);
}
