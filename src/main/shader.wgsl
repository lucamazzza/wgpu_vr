struct Matrices {
    projection: mat4x4<f32>,
    modelview: mat4x4<f32>,
    normal: mat4x4<f32>,
};

struct LightMaterial {
    lightPosition: vec4<f32>,
    lightAmbient: vec4<f32>,
    lightDiffuse: vec4<f32>,
    lightSpecular: vec4<f32>,
    matAmbient: vec4<f32>,
    matDiffuse: vec4<f32>,
    matSpecular: vec4<f32>,
    matShininess: f32,
};

@group(0) @binding(0) var<uniform> matrices: Matrices;
@group(0) @binding(1) var<uniform> lightMat: LightMaterial;

@group(1) @binding(0) var texSampler: sampler;
@group(1) @binding(1) var texData: texture_2d<f32>;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) texCoord: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) fragPosition: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) texCoord: vec2<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let viewPos = matrices.modelview * vec4<f32>(in.position, 1.0);
    out.fragPosition = viewPos.xyz;
    out.clip_position = matrices.projection * viewPos;
    out.normal = (matrices.normal * vec4<f32>(in.normal, 0.0)).xyz;
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texel = textureSample(texData, texSampler, in.texCoord);
    var fragColor = lightMat.matAmbient.xyz * lightMat.lightAmbient.xyz;
    let norm = normalize(in.normal);
    let lightDir = normalize(lightMat.lightPosition.xyz - in.fragPosition);
    let nDotL = dot(norm, lightDir);
    if (nDotL > 0.0) {
        fragColor += lightMat.matDiffuse.xyz * nDotL * lightMat.lightDiffuse.xyz;
        let viewDir = normalize(-in.fragPosition); 
        let halfVector = normalize(lightDir + viewDir);
        let nDotHV = max(dot(norm, halfVector), 0.0);
        fragColor += lightMat.matSpecular.xyz * pow(nDotHV, lightMat.matShininess) * lightMat.lightSpecular.xyz;
    }
    return texel * vec4<f32>(fragColor, 1.0);
}
