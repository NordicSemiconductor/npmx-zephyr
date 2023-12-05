.. _npmx_zephyr_release_notes:

Release notes
#############

See the release notes for the information about specific npmx-zephyr releases.

[1.0.0] - 2023-12-13
---------------------

Added
~~~~~

- Added shell commands:

    - `npmx adc meas ibat`
    - `npmx buck active_discharge get`
    - `npmx buck active_discharge set`
    - `npmx charger die_temp resume get`
    - `npmx charger die_temp resume set`
    - `npmx charger die_temp status get`
    - `npmx charger die_temp stop get`
    - `npmx charger die_temp stop set`
    - `npmx charger die_temp ntc_resistance cold get`
    - `npmx charger die_temp ntc_resistance cold set`
    - `npmx charger die_temp ntc_resistance cool get`
    - `npmx charger die_temp ntc_resistance cool set`
    - `npmx charger die_temp ntc_resistance warm get`
    - `npmx charger die_temp ntc_resistance warm set`
    - `npmx charger die_temp ntc_resistance hot get`
    - `npmx charger die_temp ntc_resistance hot set`
    - `npmx charger die_temp ntc_temperature cold get`
    - `npmx charger die_temp ntc_temperature cold set`
    - `npmx charger die_temp ntc_temperature cool get`
    - `npmx charger die_temp ntc_temperature cool set`
    - `npmx charger die_temp ntc_temperature warm get`
    - `npmx charger die_temp ntc_temperature warm set`
    - `npmx charger die_temp ntc_temperature hot get`
    - `npmx charger die_temp ntc_temperature hot set`
    - `npmx charger discharging_current get`
    - `npmx charger discharging_current set`
    - `npmx charger module full_cool get`
    - `npmx charger module full_cool set`
    - `npmx charger termination_voltage warm get`
    - `npmx charger termination_voltage warm set`
    - `npmx gpio config debounce get`
    - `npmx gpio config debounce set`
    - `npmx gpio config drive get`
    - `npmx gpio config drive set`
    - `npmx gpio config mode get`
    - `npmx gpio config mode set`
    - `npmx gpio config open_drain get`
    - `npmx gpio config open_drain set`
    - `npmx gpio config pull get`
    - `npmx gpio config pull set`
    - `npmx gpio status get`
    - `npmx gpio type get`
    - `npmx ldsw active_discharge get`
    - `npmx ldsw active_discharge set`
    - `npmx ldsw gpio index get`
    - `npmx ldsw gpio index set`
    - `npmx ldsw gpio polarity get`
    - `npmx ldsw gpio polarity set`
    - `npmx ldsw soft_start current get`
    - `npmx ldsw soft_start current set`
    - `npmx ldsw soft_start enable get`
    - `npmx ldsw soft_start enable set`
    - `npmx led mode get`
    - `npmx led mode set`
    - `npmx led state set`
    - `npmx pof polarity get`
    - `npmx pof polarity set`
    - `npmx pof status get`
    - `npmx pof status set`
    - `npmx pof threshold get`
    - `npmx pof threshold set`
    - `npmx ship config inv_polarity get`
    - `npmx ship config inv_polarity set`
    - `npmx ship config time get`
    - `npmx ship config time set`
    - `npmx ship mode hibernate`
    - `npmx ship mode ship`
    - `npmx ship reset long_press get`
    - `npmx ship reset long_press set`
    - `npmx ship reset two_buttons get`
    - `npmx ship reset two_buttons set`
    - `npmx timer config compare get`
    - `npmx timer config compare set`
    - `npmx timer config mode get`
    - `npmx timer config mode set`
    - `npmx timer config prescaler get`
    - `npmx timer config prescaler set`
    - `npmx timer config strobe`
    - `npmx timer disable`
    - `npmx timer enable`
    - `npmx timer watchdog kick`
    - `npmx vbusin current_limit get`
    - `npmx vbusin current_limit set`
    - `npmx vbusin status cc get`

- Added `CONFIG_NPMX_RESTORE_VALUES` Kconfig option that allows for restoring values from PMIC during npmx initialization.

Changed
~~~~~~~

- Updated the npmx version to 1.0.0.
- Updated the nrfxlib version to 2.5.0.
- `CONFIG_NPMX_DEVICE_NPM1300_ENG_C` Kconfig replaced with `CONFIG_NPMX_DEVICE_NPM1300`.
- `nordic,npm1300-eng-c` devicetree binding replaced with `nordic,npmx-npm1300`.
- Interrupt pins (`host-int-gpios` and `pmic-int-pin`) are now optional in a devicetree.
- Split the `buck` sample into `buck_output_simple`, `buck_output_advanced`, `buck_pins`, and `buck_retention`.
- Aligned samples and shell to the following changes:

    - `npmx_adc_ntc_set()` changed to `npmx_adc_ntc_config_set()`.
    - Battery temperature calculation moved to the ADC driver.
    - `NPM1300_ENG_C` define replaced with `NPM1300`.
    - Other minor npmx API changes - see npmx v1.0.0 changelog for details.
    - `npmx_core_init()` API.
    - Calling `npmx_timer_task_trigger()` with `NPMX_TIMER_TASK_STROBE` is no longer needed after `npmx_timer_config_set()`.

- Reduced sleep time for voltage stabilization in LDO example from 1 second to 100 ms.
- Renamed shell commands:

    - `npmx adc meas take vbat` to `npmx adc meas vbat`.
    - `npmx buck set` to `npmx buck status set`.
    - `npmx buck status power get` to `npmx buck status get`.
    - `npmx buck vout {get, set}` to `npmx buck vout_select {get, set}`.
    - `npmx charger charger_current {get, set}` to `npmx charget charging_current {get, set}`.
    - `npmx charger module ntc {get, set}` to `npmx charger module ntc_limits {get, set}`.
    - `npmx charger status get` to `npmx charger status all get`.
    - `npmx charger trickle {get, set}` to `npmx charger trickle_voltage {get, set}`. They now accept integer values instead of enumerations.
    - `npmx errlog check` to `npmx errlog get`.
    - `npmx vbusin vbus status get` to `npmx vbusin status connected get`.

- Split shell commands:

    - `npmx adc ntc {get, set}` into `npmx adc ntc type {get, set}` and `npmx adc ntc beta {get, set}`.
    - `npmx buck gpio retention {get, set}` into `npmx buck gpio retention index {get, set}` and `npmx buck gpio retention polarity {get, set}`.
    - `npmx buck gpio on_off {get, set}` into `npmx buck gpio on_off index {get, set}` and `npmx buck gpio on_off polarity {get, set}`.
    - `npmx buck gpio pwm_force {get, set}` into `npmx buck gpio pwm_force index {get, set}` and `npmx buck gpio pwm_force polarity {get, set}`.

- Replaced error messages in `npmx errlog get` with register field names.
- Refactored shell arguments parsing.
- Refactored shell commands and divided them into separate files.
- Minor fixes and improvements in shell commands.

Fixed
~~~~~

- Fixed PyYAML and Sphinx versions in requirements.
- Fixed an issue in the Shell sample where POF status, threshold, and enable were overwritten during initialization.
- Fixed an issue in the Shell sample where LDSW active discharge enable was overwritten during initialization.
- Minor fixes and improvements in the documentation.

[0.7.0] - 2023-07-12
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

    - A new format for convert functions.
    - `npmx_adc_meas_get()` API.
    - A new backend handling method.
- Removed all unused symbols.
- Added checking for a charger status when setting NTC with the `npmx adc ntc set` shell command.

Fixed
~~~~~

- Fixed documentation building by removing the path dependency.
- Fixed shell commands to prevent accepting invalid inputs.
- Minor fixes and improvements in documentation.

[0.6.0] - 2023-06-22
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
