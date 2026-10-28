"""Blender Cycles backend for the optional rendering-fidelity references."""

import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector


def gltf_vector(value):
	"""Convert a glTF Y-up vector to Blender's Z-up coordinates."""
	return Vector((value[0], -value[2], value[1]))


def gltf_rotation(value):
	"""Convert a glTF quaternion to Blender's Z-up coordinate frame."""
	change_basis = Matrix.Rotation(math.pi / 2, 4, "X")
	rotation = Quaternion((value[3], value[0], value[1], value[2])).to_matrix().to_4x4()
	return (change_basis @ rotation @ change_basis.inverted()).to_quaternion()


def _clear_scene():
	bpy.ops.object.select_all(action="SELECT")
	bpy.ops.object.delete(use_global=False)
	for collection in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights, bpy.data.images, bpy.data.armatures):
		for data in list(collection):
			if data.users == 0:
				collection.remove(data)


def _snapshot_and_freeze(objects, pose, seconds):
	if pose.get("type", "rest") == "rest":
		for obj in objects:
			if obj.animation_data:
				obj.animation_data_clear()
			if obj.type == "ARMATURE":
				obj.data.pose_position = "REST"
		bpy.context.view_layer.update()
		return
	if pose.get("type") != "clip" or pose.get("clip") != 0:
		raise RuntimeError(f"unsupported glTF animation pose: {pose}")
	frame_time = float(seconds) * bpy.context.scene.render.fps
	frame = math.floor(frame_time)
	bpy.context.scene.frame_set(frame, subframe=frame_time - frame)
	bpy.context.view_layer.update()
	depsgraph = bpy.context.evaluated_depsgraph_get()
	for obj in objects:
		evaluated = obj.evaluated_get(depsgraph)
		obj.location = evaluated.location.copy()
		obj.rotation_mode = "QUATERNION"
		obj.rotation_quaternion = evaluated.rotation_quaternion.copy()
		obj.scale = evaluated.scale.copy()
		if obj.type == "ARMATURE":
			for original_bone, evaluated_bone in zip(obj.pose.bones, evaluated.pose.bones):
				original_bone.location = evaluated_bone.location.copy()
				original_bone.rotation_mode = "QUATERNION"
				original_bone.rotation_quaternion = evaluated_bone.rotation_quaternion.copy()
				original_bone.scale = evaluated_bone.scale.copy()
	for obj in objects:
		if obj.animation_data:
			obj.animation_data_clear()
		if obj.type == "ARMATURE" and obj.data.animation_data:
			obj.data.animation_data_clear()
	bpy.context.scene.frame_set(0)
	bpy.context.view_layer.update()


def _import_instance(gltf_path, transform, pose):
	before = set(bpy.data.objects)
	bpy.ops.import_scene.gltf(filepath=str(gltf_path), import_pack_images=True)
	imported = set(bpy.data.objects) - before
	if not imported:
		raise RuntimeError(f"glTF importer produced no objects for {gltf_path}")
	_snapshot_and_freeze(imported, pose, pose.get("time", 0.0))
	roots = [obj for obj in imported if obj.parent not in imported]
	instance = bpy.data.objects.new("reference-instance", None)
	bpy.context.scene.collection.objects.link(instance)
	for root in roots:
		root.parent = instance
		root.matrix_parent_inverse = Matrix.Identity(4)
	instance.location = gltf_vector(transform.get("translation", [0, 0, 0]))
	instance.rotation_mode = "QUATERNION"
	instance.rotation_quaternion = gltf_rotation(transform.get("rotation", [0, 0, 0, 1]))
	instance.scale = Vector(transform.get("scale", [1, 1, 1]))


def _look_at(obj, target):
	obj.rotation_euler = (gltf_vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def _environment(scene, scene_path):
	world = bpy.data.worlds.new("reference-world")
	bpy.context.scene.world = world
	world.use_nodes = True
	nodes = world.node_tree.nodes
	background = nodes.get("Background")
	background.inputs["Color"].default_value = (*scene.get("environment", {}).get("radiance", [0.18, 0.18, 0.18]), 1.0)
	environment = scene.get("environment", {})
	if environment.get("type") == "equirectangular":
		path = (scene_path.parent / environment["path"]).resolve()
		if not path.is_file():
			raise RuntimeError(f"environment image is missing: {path}")
		image = bpy.data.images.load(str(path), check_existing=True)
		if environment.get("encoding", "srgb").lower() == "srgb":
			image.colorspace_settings.name = "sRGB"
		texture = nodes.new("ShaderNodeTexEnvironment")
		texture.image = image
		coordinates = nodes.new("ShaderNodeTexCoord")
		mapping = nodes.new("ShaderNodeMapping")
		mapping.inputs["Rotation"].default_value[2] = math.radians(float(environment.get("rotation_degrees", 0.0)))
		world.node_tree.links.new(coordinates.outputs["Generated"], mapping.inputs["Vector"])
		world.node_tree.links.new(mapping.outputs["Vector"], texture.inputs["Vector"])
		world.node_tree.links.new(texture.outputs["Color"], background.inputs["Color"])
	elif environment.get("type", "neutral") != "neutral":
		raise RuntimeError(f"unsupported reference environment: {environment}")
	background.inputs["Strength"].default_value = float(environment.get("radiance_scale", 1.0))


def _add_floor(floor):
	if not floor or not floor.get("enabled", True):
		return
	center = gltf_vector(floor.get("position", floor.get("center", [0, 0, 0])))
	size = floor.get("size", 20.0)
	if isinstance(size, (int, float)):
		dimensions = (float(size), 0.08, float(size))
	else:
		dimensions = (float(size[0]), float(size[2]), float(size[1]))
	bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
	plane = bpy.context.object
	plane.name = "reference-floor"
	plane.dimensions = dimensions
	bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
	material = bpy.data.materials.new("reference-floor-material")
	color = floor.get("base_color", floor.get("color", [0.18, 0.18, 0.18, 1.0]))
	material.diffuse_color = tuple(color)
	material.use_nodes = True
	material.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = material.diffuse_color
	material.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = float(floor.get("roughness", 0.85))
	plane.data.materials.append(material)
	plane.visible_shadow = bool(floor.get("casts_shadow", True))


def _lights(lights):
	for index, spec in enumerate(lights or []):
		data = bpy.data.lights.new(f"reference-point-{index}", type="POINT")
		# glTF/Krisp intensities are candela for an isotropic point light. Blender
		# point-light power is radiant watts: convert using 683 lm/W for neutral
		# white light and 4*pi steradians. This is an explicit photometric mapping.
		data.energy = float(spec["intensity"]) * (4.0 * math.pi / 683.0)
		data.color = tuple(spec.get("color", [1, 1, 1]))
		data.use_shadow = bool(spec.get("casts_shadow", False))
		obj = bpy.data.objects.new(data.name, data)
		bpy.context.scene.collection.objects.link(obj)
		obj.location = gltf_vector(spec["position"])


def render(case_path, gltf_path, scene, output_path):
	case_path, gltf_path, output_path = Path(case_path), Path(gltf_path), Path(output_path)
	_clear_scene()
	resolution = scene.get("resolution", [640, 480])
	settings = scene.get("render", {})
	scene_data = bpy.context.scene
	scene_data.render.engine = "CYCLES"
	scene_data.cycles.device = "CPU"
	scene_data.cycles.samples = int(settings.get("samples", 32))
	scene_data.cycles.use_denoising = bool(settings.get("denoise", False))
	scene_data.render.resolution_x, scene_data.render.resolution_y = map(int, resolution)
	scene_data.render.resolution_percentage = 100
	scene_data.render.image_settings.file_format = "PNG"
	scene_data.render.image_settings.color_mode = "RGBA"
	scene_data.render.film_transparent = False
	scene_data.view_settings.view_transform = "Standard"
	scene_data.view_settings.look = "Medium High Contrast"
	scene_data.view_settings.exposure = float(settings.get("exposure_ev", 0.0))
	scene_data.view_settings.gamma = 1.0
	scene_data.render.fps = 30

	_environment(scene, case_path)
	_add_floor(scene.get("floor"))
	_lights(scene.get("point_lights"))
	instances = scene.get("instances", [])
	poses = scene.get("poses", [])
	if not instances or len(instances) != len(poses):
		raise RuntimeError("reference_scene.instances and capture poses must have matching non-zero lengths")
	for transform, pose in zip(instances, poses):
		_import_instance(gltf_path, transform, pose)

	camera_spec = scene["camera"]
	camera_data = bpy.data.cameras.new("reference-camera")
	camera = bpy.data.objects.new("reference-camera", camera_data)
	scene_data.collection.objects.link(camera)
	scene_data.camera = camera
	camera.location = gltf_vector(camera_spec["from"])
	_look_at(camera, camera_spec["target"])
	camera_data.sensor_fit = "VERTICAL"
	camera_data.sensor_height = 24.0
	camera_data.lens = camera_data.sensor_height / (2.0 * math.tan(math.radians(float(camera_spec["vertical_fov_degrees"])) / 2.0))

	output_path.parent.mkdir(parents=True, exist_ok=True)
	scene_data.render.filepath = str(output_path)
	bpy.ops.render.render(write_still=True)
	return {
		"renderer_settings": {
			"engine": "Cycles CPU",
			"samples": scene_data.cycles.samples,
			"denoise": scene_data.cycles.use_denoising,
			"view_transform": scene_data.view_settings.view_transform,
			"look": scene_data.view_settings.look,
			"exposure_ev": scene_data.view_settings.exposure,
			"gamma": scene_data.view_settings.gamma,
			"point_light_mapping": "glTF/Krisp candela to Blender radiometric watts: cd * 4*pi/683, assuming 683 lm/W neutral-white efficacy",
			"shadow_support": True,
		},
	}


def _main():
	if "--" not in sys.argv:
		raise SystemExit("expected Blender arguments after --")
	case_path, gltf_path, scene_json, output_path = sys.argv[sys.argv.index("--") + 1:]
	metadata = render(case_path, gltf_path, json.loads(scene_json), output_path)
	print("KRISP_REFERENCE_METADATA=" + json.dumps(metadata, sort_keys=True))


if __name__ == "__main__":
	_main()
