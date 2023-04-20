#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec2 attribs; // barycentric coordinates of the triangle hit by a ray

layout(set=1, binding=0) uniform GlobalUniformBufferObject
{
	mat4 view; // camera
	mat4 proj;
	vec3 view_pos; // camera eye (not focus object)
	vec3 light_pos;
	float lighting_scalar;
} gubo;

struct Vertex
{
	vec3 position;
	vec3 color;
	vec2 texCoord;
	vec3 normal;
};
const uint VERTEX_SIZE = 11;

struct BufferMapEntry
{
	uint vertex_offset;
	uint index_offset;
	uint uniform_offset;
};
const uint BUFFER_MAP_ENTRY_SIZE = 12;

layout(set=2, binding=0, scalar) buffer Offsets { BufferMapEntry entries[]; } offsets; // Maps object id to offset in vertices and indices buffers
layout(set=2, binding=1, scalar) buffer Vertices { float v[]; } vertices; // Positions of an object
layout(set=2, binding=2, scalar) buffer Indices { uint i[]; } indices; // Triangle indices

Vertex unpack(uint idx)
{
	BufferMapEntry entry = offsets.entries[gl_InstanceCustomIndexEXT];
	entry.vertex_offset /= 4;
	entry.index_offset /= 4;
	entry.uniform_offset /= 4;

	const uint index = indices.i[entry.index_offset + idx];
	const uint offset = entry.vertex_offset + index * VERTEX_SIZE;
	Vertex v;
	v.position = vec3(vertices.v[offset + 0], vertices.v[offset + 1], vertices.v[offset + 2]);
	v.color = vec3(vertices.v[offset + 3], vertices.v[offset + 4], vertices.v[offset + 5]);
	v.texCoord = vec2(vertices.v[offset + 6], vertices.v[offset + 7]);
	v.normal = vec3(vertices.v[offset + 8], vertices.v[offset + 9], vertices.v[offset + 10]);
	return v;
}

void main()
{
	Vertex v1 = unpack(gl_PrimitiveID * 3 + 0);
	Vertex v2 = unpack(gl_PrimitiveID * 3 + 1);
	Vertex v3 = unpack(gl_PrimitiveID * 3 + 2);

	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	// gl_ObjectToWorldEXT is generated from TLAS transform, BLAS doesn't have transforms
	const vec3 pos = v1.position * barycentrics.x + v2.position * barycentrics.y + v3.position * barycentrics.z;
	const vec3 world_pos = (gl_ObjectToWorldEXT * vec4(pos, 1.0)).xyz;

	const vec3 normal = v1.normal * barycentrics.x + v2.normal * barycentrics.y + v3.normal * barycentrics.z;
	const vec3 world_normal = normalize((gl_ObjectToWorldEXT * vec4(normal, 0.0)).xyz);

	const vec3 color = v1.color * barycentrics.x + v2.color * barycentrics.y + v3.color * barycentrics.z;

    const vec3 lightDir = normalize(gubo.light_pos - world_pos);
    const float diff = max(dot(world_normal, lightDir), 0.0);
    const vec3 diffuse = diff * color * gubo.lighting_scalar;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(gubo.view_pos - world_pos);
    vec3 reflectDir = reflect(-lightDir, world_normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	const vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 specular = specularStrength * spec * gubo.lighting_scalar * light_color;

	hitValue = diffuse + specular;
}
