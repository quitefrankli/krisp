skybox_faces = files(
	'../../resources/default/textures/skybox/right.jpg',
	'../../resources/default/textures/skybox/left.jpg',
	'../../resources/default/textures/skybox/top.jpg',
	'../../resources/default/textures/skybox/bottom.jpg',
	'../../resources/default/textures/skybox/front.jpg',
	'../../resources/default/textures/skybox/back.jpg')

default_environment_lighting = custom_target(
	'krisp_default_environment_lighting',
	input: skybox_faces,
	output: 'default_environment.krisp-ibl',
	command: [environment_map_precompute, '@OUTPUT@', '@INPUT@'],
	build_by_default: true)

executable(meson.project_name(),
		   'krisp.cpp',
		   'krisp_ui.cpp',
		   dependencies: krisp_core,
		   link_depends: default_environment_lighting,
		   install: true,
		   install_dir: install_dir,
		   cpp_args: [
			   '-DPROJECT_NAME="krisp"',
			   '-DPROJECT_ENVIRONMENT_LIGHTING_ASSET="@0@"'.format(
				   default_environment_lighting.full_path())])
