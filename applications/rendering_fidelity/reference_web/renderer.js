import { GltfState, GltfView } from "./node_modules/@khronosgroup/gltf-viewer/dist/gltf-viewer.module.js";
import * as mat4 from "./node_modules/gl-matrix/esm/mat4.js";

const canvas = document.querySelector("#reference-canvas");

function base64Bytes(value) {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; ++i) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

function pushBytes(existing, addition) {
  const aligned = (existing.length + 3) & ~3;
  const result = new Uint8Array(aligned + addition.length);
  result.set(existing);
  result.set(addition, aligned);
  return { bytes: result, offset: aligned };
}

function addBinary(document, bytes, target) {
	const buffers = document.buffers ?? (document.buffers = [{ byteLength: 0, uri: "data:application/octet-stream;base64," }]);
	if (buffers.length !== 1) throw new Error("reference adapter requires one glTF buffer");
	const oldBytes = buffers[0].uri ? base64Bytes(buffers[0].uri.slice(buffers[0].uri.indexOf(",") + 1)) : new Uint8Array();
  const combined = pushBytes(oldBytes, bytes);
	let binary = "";
	for (let start = 0; start < combined.bytes.length; start += 0x8000) {
		binary += String.fromCharCode(...combined.bytes.subarray(start, start + 0x8000));
	}
	buffers[0].uri = `data:application/octet-stream;base64,${btoa(binary)}`;
  buffers[0].byteLength = combined.bytes.length;
  const views = document.bufferViews ?? (document.bufferViews = []);
  const viewIndex = views.length;
  views.push({ buffer: 0, byteOffset: combined.offset, byteLength: bytes.length, ...(target ? { target } : {}) });
  return viewIndex;
}

function appendFloatAccessor(document, values) {
  const bytes = new Uint8Array(values.length * 4);
  const view = new DataView(bytes.buffer);
  values.forEach((value, index) => view.setFloat32(index * 4, value, true));
  const bufferView = addBinary(document, bytes, 34962);
  const accessorIndex = (document.accessors ??= []).length;
  document.accessors.push({
    bufferView,
    componentType: 5126,
    count: values.length,
    type: "SCALAR",
    min: [Math.min(...values)],
    max: [Math.max(...values)],
  });
  return accessorIndex;
}

function shiftedInput(document, accessorIndex, shift) {
  const accessor = document.accessors[accessorIndex];
  if (accessor.componentType !== 5126 || accessor.type !== "SCALAR" || accessor.sparse) {
    throw new Error("animation sampler input must be a float scalar accessor");
  }
  const view = document.bufferViews[accessor.bufferView];
  const bytes = base64Bytes(document.buffers[view.buffer].uri.split(",", 2)[1]);
  const offset = (view.byteOffset ?? 0) + (accessor.byteOffset ?? 0);
  const reader = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const values = Array.from({ length: accessor.count }, (_, index) => reader.getFloat32(offset + index * 4, true) + shift);
  return appendFloatAccessor(document, values);
}

async function inlineModelResources(document, baseUrl) {
	const assetBaseUrl = new URL(baseUrl, window.location.href);
  for (const resource of [...(document.buffers ?? []), ...(document.images ?? [])]) {
    if (!resource.uri || resource.uri.startsWith("data:")) continue;
    const response = await fetch(new URL(resource.uri, assetBaseUrl));
    if (!response.ok) throw new Error(`could not load glTF dependency ${resource.uri}: ${response.status}`);
    const bytes = new Uint8Array(await response.arrayBuffer());
    let binary = "";
    for (let start = 0; start < bytes.length; start += 0x8000) {
      binary += String.fromCharCode(...bytes.subarray(start, start + 0x8000));
    }
			const lowerUri = resource.uri.toLowerCase();
			const mime = resource.mimeType ?? (lowerUri.endsWith(".png") ? "image/png" : (lowerUri.endsWith(".jpg") || lowerUri.endsWith(".jpeg")) ? "image/jpeg" : "application/octet-stream");
    resource.uri = `data:${mime};base64,${btoa(binary)}`;
  }
}

function addFloor(document, floor) {
  if (!floor || floor.enabled === false) return null;
  const size = Array.isArray(floor.size) ? floor.size : [floor.size ?? 20, 0.08, floor.size ?? 20];
  const [x, y, z] = size.map((value) => Number(value) / 2);
  const faces = [
    [[0, 1, 0], [[-x, y, -z], [-x, y, z], [x, y, z], [x, y, -z]]],
    [[0, -1, 0], [[-x, -y, -z], [x, -y, -z], [x, -y, z], [-x, -y, z]]],
    [[1, 0, 0], [[x, -y, -z], [x, y, -z], [x, y, z], [x, -y, z]]],
    [[-1, 0, 0], [[-x, -y, -z], [-x, -y, z], [-x, y, z], [-x, y, -z]]],
    [[0, 0, 1], [[-x, -y, z], [x, -y, z], [x, y, z], [-x, y, z]]],
    [[0, 0, -1], [[-x, -y, -z], [-x, y, -z], [x, y, -z], [x, -y, -z]]],
  ];
  const vertices = new Float32Array(faces.flatMap(([, positions]) => positions.flat()));
  const normals = new Float32Array(faces.flatMap(([normal]) => Array(4).fill(normal).flat()));
  const indices = new Uint16Array(Array.from({ length: 6 }, (_, face) => {
    const start = face * 4;
    return [start, start + 1, start + 2, start, start + 2, start + 3];
  }).flat());
  const positionView = addBinary(document, new Uint8Array(vertices.buffer), 34962);
  const normalView = addBinary(document, new Uint8Array(normals.buffer), 34962);
  const indexView = addBinary(document, new Uint8Array(indices.buffer), 34963);
  const accessors = document.accessors ?? (document.accessors = []);
  const position = accessors.length;
  accessors.push({ bufferView: positionView, componentType: 5126, count: 24, type: "VEC3", min: [-x, -y, -z], max: [x, y, z] });
  const normal = accessors.length;
  accessors.push({ bufferView: normalView, componentType: 5126, count: 24, type: "VEC3" });
  const index = accessors.length;
  accessors.push({ bufferView: indexView, componentType: 5123, count: 36, type: "SCALAR" });
  const materials = document.materials ?? (document.materials = []);
  const material = materials.length;
  const color = floor.base_color ?? floor.color ?? [0.18, 0.18, 0.18, 1];
  materials.push({
    name: "reference-floor",
    pbrMetallicRoughness: {
      baseColorFactor: color.length === 4 ? color : [...color, 1],
      metallicFactor: Number(floor.metallic ?? 0),
      roughnessFactor: Number(floor.roughness ?? 0.85),
    },
  });
  const meshes = document.meshes ?? (document.meshes = []);
  const mesh = meshes.length;
  meshes.push({ primitives: [{ attributes: { POSITION: position, NORMAL: normal }, indices: index, material }] });
  const nodes = document.nodes ?? (document.nodes = []);
  const node = nodes.length;
  nodes.push({ name: "reference-floor", mesh, translation: floor.position ?? floor.center ?? [0, 0, 0] });
  return node;
}

function addPointLights(document, lights) {
  const extensions = document.extensions ?? (document.extensions = {});
  const punctual = extensions.KHR_lights_punctual ?? (extensions.KHR_lights_punctual = { lights: [] });
  const nodes = document.nodes ?? (document.nodes = []);
  const result = [];
  for (const light of lights ?? []) {
    const index = punctual.lights.length;
    punctual.lights.push({ type: "point", color: light.color ?? [1, 1, 1], intensity: light.intensity });
    const node = nodes.length;
    nodes.push({
      name: `reference-point-light-${index}`,
      translation: light.position,
      extensions: { KHR_lights_punctual: { light: index } },
    });
    result.push(node);
  }
  if ((lights ?? []).length) {
    const used = document.extensionsUsed ?? (document.extensionsUsed = []);
    if (!used.includes("KHR_lights_punctual")) used.push("KHR_lights_punctual");
    const required = document.extensionsRequired ?? (document.extensionsRequired = []);
    if (!required.includes("KHR_lights_punctual")) required.push("KHR_lights_punctual");
  }
  return result;
}

function addInstances(document, transforms, poses) {
  const activeScene = document.scenes?.[document.scene ?? 0];
  if (!activeScene) throw new Error("asset has no active glTF scene");
  const sourceNodes = document.nodes.map((node) => structuredClone(node));
  const sourceRoots = [...(activeScene.nodes ?? [])];
  const sourceAnimations = (document.animations ?? []).map((animation) => structuredClone(animation));
  const sourceSkins = (document.skins ?? []).map((skin) => structuredClone(skin));
  const sceneRoots = [];
	const animationIndices = [];
  const clipTimes = poses.filter((pose) => pose.type === "clip").map((pose) => Number(pose.time));
  const fixedTime = clipTimes.length ? Math.max(...clipTimes) : 0;

  for (let instanceIndex = 0; instanceIndex < transforms.length; ++instanceIndex) {
    const nodeBase = document.nodes.length;
    const skinBase = (document.skins ??= []).length;
    document.skins.push(...sourceSkins.map((skin) => structuredClone(skin)));
    const remap = sourceNodes.map((_, index) => nodeBase + index);
    for (let sourceIndex = 0; sourceIndex < sourceNodes.length; ++sourceIndex) {
      const clone = structuredClone(sourceNodes[sourceIndex]);
      if (clone.children) clone.children = clone.children.map((child) => remap[child]);
      if (clone.skin !== undefined) clone.skin += skinBase;
      document.nodes.push(clone);
    }
    for (let i = 0; i < sourceSkins.length; ++i) {
      const skin = document.skins[skinBase + i];
      skin.joints = skin.joints.map((joint) => remap[joint]);
      if (skin.skeleton !== undefined) skin.skeleton = remap[skin.skeleton];
    }
    const pose = poses[instanceIndex] ?? { type: "rest" };
    if (pose.type === "clip") {
      const animation = sourceAnimations[Number(pose.clip)];
      if (!animation) throw new Error(`asset has no animation clip ${pose.clip}`);
      const copy = structuredClone(animation);
      const shift = fixedTime - Number(pose.time);
      copy.samplers = copy.samplers.map((sampler) => ({
        ...sampler,
        input: shift === 0 ? sampler.input : shiftedInput(document, sampler.input, shift),
      }));
      copy.channels = copy.channels
        .filter((channel) => channel.target.node !== undefined)
        .map((channel) => ({ ...channel, target: { ...channel.target, node: remap[channel.target.node] } }));
		animationIndices.push((document.animations ??= []).length);
		document.animations.push(copy);
    } else if (pose.type !== "rest") {
      throw new Error(`unsupported pose ${JSON.stringify(pose)}`);
    }
    const wrapper = document.nodes.length;
    document.nodes.push({
      name: `reference-instance-${instanceIndex}`,
      children: sourceRoots.map((index) => remap[index]),
      translation: transforms[instanceIndex].translation ?? [0, 0, 0],
      rotation: transforms[instanceIndex].rotation ?? [0, 0, 0, 1],
      scale: transforms[instanceIndex].scale ?? [1, 1, 1],
    });
    sceneRoots.push(wrapper);
  }
	return { sceneRoots, fixedTime, animationIndices };
}

async function renderJob(job) {
  canvas.width = job.scene.resolution?.[0] ?? 640;
  canvas.height = job.scene.resolution?.[1] ?? 480;
  const view = new GltfView(canvas.getContext("webgl2", { preserveDrawingBuffer: true }));
  const loader = view.createResourceLoader(undefined, undefined, "/applications/rendering_fidelity/reference_web/node_modules/@khronosgroup/gltf-viewer/dist/libs/");
  const response = await fetch(job.assetUrl);
  if (!response.ok) throw new Error(`could not fetch asset ${job.assetUrl}: ${response.status}`);
  const document = await response.json();
  await inlineModelResources(document, job.assetUrl);
  const layout = addInstances(document, job.scene.instances, job.scene.poses);
  const floor = addFloor(document, job.scene.floor);
  const lightNodes = addPointLights(document, job.scene.point_lights);
  const scene = document.scenes[document.scene ?? 0];
  scene.nodes = [...layout.sceneRoots, ...(floor === null ? [] : [floor]), ...lightNodes];
  const state = view.createState();
  state.gltf = await loader.loadGltf(["reference.gltf", new File([JSON.stringify(document)], "reference.gltf")]);
  state.sceneIndex = document.scene ?? 0;
	state.animationIndices = layout.animationIndices;
	if (layout.animationIndices.length) {
    state.animationTimer.setFixedTime(layout.fixedTime);
  }
  if (job.environmentBytes) {
    state.environment = await loader.loadEnvironment(base64Bytes(job.environmentBytes).buffer);
  }
  const camera = job.scene.camera;
  state.userCamera.setVerticalFoV((Number(camera.vertical_fov_degrees) * Math.PI) / 180);
  state.userCamera.lookAt(camera.from, camera.target);
  // v1.1.0 lookAt stores a view matrix, while getViewMatrix inverts the
  // stored transform. Convert it to the camera world transform first.
  mat4.invert(state.userCamera.transform, state.userCamera.transform);
  state.renderingParameters.usePunctual = true;
  state.renderingParameters.useIBL = Boolean(state.environment);
  state.renderingParameters.renderEnvironmentMap = Boolean(state.environment);
  state.renderingParameters.environmentRotation = Number(job.scene.environment?.rotation_degrees ?? 0);
  state.renderingParameters.exposure = 2 ** Number(job.scene.render?.exposure_ev ?? 0);
  state.renderingParameters.toneMap = GltfState.ToneMaps.KHR_PBR_NEUTRAL;
  view.renderFrame(state, canvas.width, canvas.height);
  await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
  const gl = canvas.getContext("webgl2");
  const renderer = gl.getExtension("WEBGL_debug_renderer_info")
    ? gl.getParameter(gl.getExtension("WEBGL_debug_renderer_info").UNMASKED_RENDERER_WEBGL)
    : gl.getParameter(gl.RENDERER);
  return {
    png: canvas.toDataURL("image/png").split(",", 2)[1],
    gpu_renderer: renderer,
    reference_renderer: "Khronos glTF Sample Renderer / WebGL 2",
    capabilities: { cast_shadows: false, floor: Boolean(job.scene.floor), point_lights: (job.scene.point_lights ?? []).length },
    environment_conversion: job.environmentConversion ?? null,
    renderer_settings: {
      tone_map: "KHR_PBR_NEUTRAL",
      exposure_ev: Number(job.scene.render?.exposure_ev ?? 0),
      environment_rotation_degrees: Number(job.scene.environment?.rotation_degrees ?? 0),
      ibl_enabled: Boolean(state.environment),
      ibl_intensity: state.renderingParameters.iblIntensity,
      point_light_unit: "glTF KHR_lights_punctual candela",
      shadow_support: false,
    },
  };
}

window.renderKhronosReference = renderJob;
