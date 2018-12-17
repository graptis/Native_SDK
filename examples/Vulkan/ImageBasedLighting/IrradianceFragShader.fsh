#version 320 es

#define PI 3.1415926535897932384626433832795

layout(set = 0, binding = 0) uniform mediump samplerCube envMap;
layout(location = 0) in highp vec3 inPos;
layout(location = 0) out mediump vec4 outColor;

const mediump float DELTA_THETA = 1.f;
const mediump float DELTA_PHI  = 1.f;
const mediump float HDR_SCALER = 500.0;

mediump float M(mediump vec3 N, mediump vec3 dir)
{
    return max(dot(N, dir), 0.0);
}

//http://graphicrants.blogspot.com/2009/04/rgbm-color-encoding.html
mediump vec4 RGBMEncode(mediump vec3 color) 
{
  mediump vec4 rgbm;
  color *= 1.0 / 6.0;
  rgbm.a = clamp( max( max( color.r, color.g ), max( color.b, 1e-6 ) ), 0.0, 1.0);
  rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
  rgbm.rgb = color / rgbm.a;
  return rgbm;
}

void main()
{
    mediump vec3 norm = normalize(inPos);
    const mediump float twoPI = PI * 2.0;
    const mediump float halfPI = PI * 0.5;

    mediump float numSamples = 0.;
    mediump vec3 out_col_tmp = vec3(0.);
    
    for(mediump float theta = 0.0; theta < twoPI; theta += DELTA_THETA)
    {
        for(mediump float phi  = 0.; phi < twoPI; phi += DELTA_PHI)
        {
            mediump float cosTheta = cos(theta);
            mediump float sinPhi = sin(phi);
            mediump float sinTheta = sin(theta);
            mediump float cosPhi = cos(phi);
            mediump vec3 dir = normalize(vec3(sinTheta * cosPhi, sinPhi, cosPhi * cosTheta));
            mediump vec4 tmpEnv = texture(envMap, dir);
            mediump float tmpLight = tmpEnv.a;
            
            tmpLight = HDR_SCALER * tmpLight * tmpLight + 1.0;
            tmpEnv.rgb *= tmpLight;
            
            out_col_tmp += tmpEnv.rgb * M(norm, dir);
            numSamples += 1.;
        }
    }

    outColor = RGBMEncode(out_col_tmp * PI / numSamples);
}
