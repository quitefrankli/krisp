# Custom target to run shader_compiler.sh when any file under shaders/ changes
shader_files = [
	files(meson.source_root()/'shared_code/shared_data_structures.glsl'),
	files(meson.source_root()/'shared_code/shared_data_structures.txt'),
]
foreach f : run_command('find', meson.source_root() / 'shaders', '-type', 'f').stdout().split('\n')
	if f != ''
		shader_files += files(f)
	endif
endforeach

custom_target(
	'shader_artifacts',
	input: shader_files,
	output: ['dummy'],
	command: ['bash', meson.source_root()/'shader_compiler.sh', '&&', 'touch', '@OUTPUT@'],
	depend_files: shader_files,
	build_by_default: true
)