#!/usr/bin/env bash

# Source this file from the shell to make the Krisp build helper available.
krisp()
{
	local target=krisp
	local target_was_explicit=false
	local configuration=debug
	local configuration_was_explicit=false
	local argument

	for argument in "$@"
	do
		case "$argument" in
			--debug|--release)
				local requested_configuration=${argument#--}
				if [ "$configuration_was_explicit" = true ] && [ "$configuration" != "$requested_configuration" ]
				then
					echo "usage: krisp [target] [--debug|--release]" >&2
					return 2
				fi
				configuration=$requested_configuration
				configuration_was_explicit=true
				;;
			--*)
				echo "usage: krisp [target] [--debug|--release]" >&2
				return 2
				;;
			*)
				if [ "$target_was_explicit" = true ]
				then
					echo "usage: krisp [target] [--debug|--release]" >&2
					return 2
				fi
				target=$argument
				target_was_explicit=true
				;;
		esac
	done

	local project_directory
	project_directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
	local build_directory="$project_directory/build/$configuration"
	local conan_native_file="$project_directory/build/conan/conan_meson_native.ini"

	if [ ! -f "$conan_native_file" ]
	then
		echo "Conan dependencies are not installed; run:" >&2
		echo "  conan install . -pr=conan_clang_profile --build=missing" >&2
		return 1
	fi

	if [ ! -f "$build_directory/meson-private/coredata.dat" ]
	then
		local ndebug=true
		if [ "$configuration" = debug ]
		then
			ndebug=false
		fi

		meson setup "$build_directory" "$project_directory" \
			--native-file "$conan_native_file" \
			--buildtype="$configuration" \
			-Db_ndebug="$ndebug" || return
	fi

	meson compile -C "$build_directory" "$target" -j 6 && \
		"$build_directory/applications/$target/$target"
}
