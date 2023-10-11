.. _npmx_zephyr_release_notes:

Release notes
#############

See the release notes for the information about specific npmx-zephyr releases.

[0.7.0] - 12/07/2023
---------------------

Added
~~~~~

- Added the fuel gauge sample.
- Added support for nrfxlib.
- Added shell commands:

    - `npmx buck status power get`
    - `npmx buck mode`
    - `npmx charger module ntc set`
    - `npmx charger module ntc get`
    - `npmx reset`

Changed
~~~~~~~

- Updated the npmx version to 0.7.0.
- Aligned samples and shell to the following changes:

    - A new format for convert functions
    - `npmx_adc_meas_get()` API
    - A new backend handling method
- Removed all unused symbols.
- Added checking for a charger status when setting NTC with the `npmx adc ntc set` shell command.

Fixed
~~~~~

- Fixed documentation building by removing the path dependency.
- Fixed shell commands to prevent accepting invalid inputs.
- Minor fixes and improvements in documentation.

[0.6.0] - 22/06/2023
---------------------

Added
~~~~~

- Added basic repository structure.
- Added README.md file.
- Added samples:

    - BUCK
    - CHARGER and Events
    - LDO
    - LED
    - POF
    - Shell
    - Simple
    - Timer
    - Wake-up Timer
    - Timer Watchdog
    - VBUSIN
- Added all of the required files to build the documentation.
- Added pre-commit hooks.
