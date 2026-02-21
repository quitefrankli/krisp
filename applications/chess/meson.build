chess_sources = [
  'src/chess.cpp',
  'src/board.cpp',
  'src/pieces.cpp',
  'src/state_machine.cpp'
]

executable('chess', 
           chess_sources,
           dependencies: krisp_core,
           install: true,
           install_dir: install_dir,
           cpp_args: ['-DPROJECT_NAME="chess"'])
