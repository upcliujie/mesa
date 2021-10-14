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
      output = acos(input.pos.x) + asin(input.pos.x) + atan(input.pos.x) +
               cos(input.pos.x) + sin(input.pos.y) + tan(input.pos.z);
      float s, c;
      sincos(input.pos.x, s, c);
      output += s + c;
      break;
    case 2:
      output = all(input.pos);
      break;
    case 3:
      output = any(input.pos);
      break;
    case 4:
      output = floor(input.pos.x) + ceil(input.pos.x) + round(input.pos.x) +
               frac(input.pos.x);
      output += output % 2.5;
      break;
    case 5:
      output = ddx(input.pos.x) + ddy(input.pos.x) + ddx_coarse(input.pos.x) +
               ddy_coarse(input.pos.x) + ddx_fine(input.pos.x) +
               ddy_fine(input.pos.x);
      break;
    case 6:
      output = dot(input.pos, input.pos);
      break;
  }
  return float4(output, 0.0, 1.0, 1.0);
}
