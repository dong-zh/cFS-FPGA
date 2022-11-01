project(FPGA_CTRL C)

include_directories(fsw/mission_inc)
include_directories(fsw/platform_inc)

# Create the app module
add_cfe_app(
  fpga_ctrl

  fsw/src/fpga_ctrl.c
  fsw/src/fpga_ctrl_interrupts.h
  fsw/src/fpga_ctrl_aes.h
  fsw/src/fpga_ctrl_load_bitstream.h
)

# Include the public API from sample_lib to demonstrate how
# to call library-provided functions

# add_cfe_app_dependency(fpga_ctrl sample_lib)
add_cfe_app_dependency(fpga_ctrl mmio_lib)


# Add table
add_cfe_tables(fpgaCtrlTable fsw/tables/fpga_ctrl_tbl.c)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)
