exec=$TCHEM_INSTALL_PATH/example/TChem_IgnitionZeroD_HostSingle.x
inputs=$TCHEM_INSTALL_PATH/example/data/ignition-zero-d/gri3.0

this="$exec --chemfile=$inputs/chem.inp \
            --thermfile=$inputs/therm.dat \
            --samplefile=$inputs/input.dat \
            --outputfile=IgnSolution_host_single.dat \
            --atol-newton=1e-18 \
            --rtol-newton=1e-8\
            --max-newton-iterations=20 \
            --tol-time=1e-6 \
            --dtmax=1e-3 \
            --dtmin=1e-20 \
            --tend=2 \
            --time-iterations-per-interval=10 \
            --max-time-iterations=260 " 

echo $this
eval $this