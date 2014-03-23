# MetaTM Project
# File Name: need-network.py
#
# Description: Determines whether to load network hardware.  Only do
# so if we are going to run an experiment that uses it, as networking
# slows the boot.
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2006, 2007. All Rights Reserved.
# See LICENSE file for license terms.

@benchmark_set = set(benchmarks)

@network_set = set(['specweb_cli', 'specweb_cli_primary',
                    'server', 'apt', 'scp', 'scp_nplus1',
                    'nc', 'nc_2n', 'nc_udp', 'nc_udp_2n',
                    'nc_server', 'nc_udp_server'])

@if benchmark_set & network_set :
    simenv.create_network = "yes"
                
