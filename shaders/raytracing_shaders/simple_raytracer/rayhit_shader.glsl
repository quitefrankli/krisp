#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../../library/library.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec2 attribs; // barycentric coordinates of the triangle hit by a ray

layout(set=RAYTRACING_LOW_FREQ_SET_OFFSET, binding=RAYTRACING_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(set=RAYTRACING_MESH_DATA_SET_OFFSET, binding=BUFFER_MAPPER_BINDING) buffer Offsets { BufferMapEntry entries[]; } offsets; // Maps object id to offset in vertices and indices buffers
layout(set=RAYTRACING_MESH_DATA_SET_OFFSET, binding=VERTICES_DATA_BINDING) buffer Vertices { ColorVertex v[]; } vertices; // Positions of an object
layout(set=RAYTRACING_MESH_DATA_SET_OFFSET, binding=INDICES_DATA_BINDING, scalar) buffer Indices { uint i[]; } indices; // Triangle indices

ColorVertex unpack(uint idx)
{
	BufferMapEntry entry = offsets.entries[gl_InstanceCustomIndexEXT];
	entry.vertex_offset /= 4;
	entry.index_offset /= 4;
	entry.uniform_offset /= 4;

	const uint index = indices.i[entry.index_offset + idx];
	const uint offset = entry.vertex_offset + index * COLOR_VERTEX_SIZE;

	return vertices.v[offset / 4];
}

void main()
{
	ColorVertex v1 = unpack(gl_PrimitiveID * 3 + 0);
	ColorVertex v2 = unpack(gl_PrimitiveID * 3 + 1);
	ColorVertex v3 = unpack(gl_PrimitiveID * 3 + 2);

	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	// gl_ObjectToWorldEXT is generated from TLAS transform, BLAS doesn't have transforms
	const vec3 pos = v1.pos * barycentrics.x + v2.pos * barycentrics.y + v3.pos * barycentrics.z;
	const vec3 world_pos = (gl_ObjectToWorldEXT * vec4(pos, 1.0)).xyz;

	const vec3 normal = v1.normal * barycentrics.x + v2.normal * barycentrics.y + v3.normal * barycentrics.z;
	const vec3 world_normal = normalize((gl_ObjectToWorldEXT * vec4(normal, 0.0)).xyz);

	const vec3 color = vec3(1.0, 1.0, 1.0); // TODO: use color from materials

    const vec3 lightDir = normalize(global_data.data.light_pos - world_pos);
    const float diff = max(dot(world_normal, lightDir), 0.0);
    const vec3 diffuse = diff * color * global_data.data.lighting_scalar;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(global_data.data.view_pos - world_pos);
    vec3 reflectDir = reflect(-lightDir, world_normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	const vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 specular = specularStrength * spec * global_data.data.lighting_scalar * light_color;

	hitValue = diffuse + specular;
}
