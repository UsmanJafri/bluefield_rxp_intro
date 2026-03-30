#
# Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
#
# This software product is a proprietary product of NVIDIA CORPORATION &
# AFFILIATES (the "Company") and all right, title, and interest in and to the
# software product, including all associated intellectual property rights, are
# and shall remain exclusively with the Company.
#
# This software product is governed by the End User License Agreement
# provided with the software product.
#

project('DOCA_SAMPLE', 'C', 'CPP',
	# Get version number from file.
	version: run_command(find_program('cat'),
		files('/opt/mellanox/doca/applications/VERSION'), check: true).stdout().strip(),
	license: 'Proprietary',
	default_options: ['buildtype=debug'],
	meson_version: '>= 0.61.2'
)

DOCA_PATH = '/opt/mellanox/doca'

# Comment this line to restore warnings of experimental DOCA features
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: ['c', 'cpp'])

sample_dependencies = []
# Required for all DOCA programs
sample_dependencies += dependency('doca')

sample_inc_dirs  = []
# Common DOCA logic (samples)
sample_inc_dirs += include_directories(DOCA_PATH + '/samples')
# Common DOCA logic
sample_inc_dirs += include_directories(DOCA_PATH)
# Common DOCA logic (applications)
sample_inc_dirs += include_directories(DOCA_PATH + '/applications/common/src')

executable('simple_regex_scan', [
		'simple_regex_scan.c',
		DOCA_PATH + '/samples/common.c',
		DOCA_PATH + '/applications/common/src/utils.c',
	],
	c_args : '-Wno-missing-braces',
	dependencies : sample_dependencies,
	include_directories: sample_inc_dirs,
	install: false)
