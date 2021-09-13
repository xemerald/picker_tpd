
# This is picker_tpd's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_PICKER_TPD # module id for this instance of template
InWaveRing         WAVE_RING      # shared memory ring for output wave trace
OutWaveRing        PICK_RING      # shared memory ring for output real wave trace
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# Settings for computation of spectra, include critical damping ratio in fractions &
# the specified natural period:
#

# Settings for pre-processing of trace:
#
DriftCorrectThreshold   30        # seconds waiting for D.C.

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2

# Local station list:
#
# The local list for program that will be picked. By the way, the priority of local list
# is higher than the one from remote data. And the layout should be like these example:
#
# PickSCNL  Station  Network  Channel  Location  Conv_Factor
# PickSCNL  TEST     TW       HLZ      --        0.059816       # First example
#
