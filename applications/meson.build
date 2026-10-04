internal_app_sources = {
    'krisp': ['krisp.cpp'],
    'chess': ['src/chess.cpp', 'src/board.cpp', 'src/pieces.cpp'],
    'tetris': ['tetris.cpp'],
}

foreach app_name, app_sources : internal_app_sources
    internal_app_args = [
        '-DPROJECT_NAME="@0@"'.format(app_name),
        '-DKRISP_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
        '-DKRISP_RUNTIME_DIR="@0@"'.format(meson.project_build_root() / 'runtime'),
    ]
    subdir(app_name)
endforeach
