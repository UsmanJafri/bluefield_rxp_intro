# Getting started with BlueField RXP

This is a brief guide on setting up the BlueField 3 DPU for regular expression matching using the RXP accelerator.


## Requirements:

- BlueField 3 B3240P

- **DPU DOCA**: we will be using a slightly older version: [v2.2.1](https://developer.nvidia.com/doca-2-2-1-download-archive)

	In DOCA v2.2.1, all the RXP-related modules are cleanly bundled together.

	In newer releases, regex supported is removed from DOCA in favor of DPDK. Unfortunately, the RXP compiler required to use regex matching engine (RXP) is missing from both DPDK and DOCA.

- **Host DOCA** server, we will be using DOCA [v3.1.0](https://developer.nvidia.com/doca-3-1-0-download-archive)

	Having different DOCA versions on Host and DPU is not recommended but in my experience it works fine.

	Reason: the *pnet-emerald* servers are already set up with Ubuntu 24 while DOCA v2.2.1 only supports Ubuntu 22. To avoid wiping the host server, we opt for this split version set up.

## Table of content:
- Section A - [Setting up the Host](#a)
- Section B - [Setting up the DPU](#b)
- Section C - [Enabling RXP engine](#c)
- Section D - [Compiling regular expressions](#d)
- Section E - [Build and run the sample RXP matcher program](#e)

## A. Setting up the Host<a name="a"></a>

1. Open up a Terminal and run the following commands to uninstall any previous versions of DOCA:

	```sh
	for f in $( dpkg --list | grep -E 'doca|flexio|dpa-gdbserver|dpa-stats|dpa-resource-mgmt|dpaeumgmt|dpdk-community' | awk '{print $2}' ); do echo $f ; sudo apt remove --purge $f -y ; done

	sudo /usr/sbin/ofed_uninstall.sh --force
	
	sudo apt-get autoremove
	```

2. Reboot the machine:

	```sh
	sudo reboot
	```

3. Download DOCA v2.2.1 for Host servers:

	```sh
	wget https://www.mellanox.com/downloads/DOCA/DOCA_v2.2.1/doca-host-repo-ubuntu2204_2.2.1-0.0.3.2.2.1009.1.23.07.0.5.0.0_amd64.deb

	sudo dpkg -i doca-host-repo-ubuntu2204_2.2.1-0.0.3.2.2.1009.1.23.07.0.5.0.0_amd64.deb

	sudo apt update

	sudo apt install doca-runtime doca-sdk doca-tools
	```

4. Reboot the machine:

	```sh
	sudo reboot
	```

## B. Setting up the DPU<a name="b"></a>

1. Restart rshim service, to estabilish management connection to DPU.
	```sh
	sudo systemctl restart rshim
	```

2. Find the virtual management device ID. (Probably going to be `rshim0`.)

	```sh
	sudo ls -la /dev/ | grep rshim
	```

3. Download DOCA v2.2.1 BFB image:

	```sh
	wget https://content.mellanox.com/BlueField/BFBs/Ubuntu22.04/DOCA_2.2.1_BSP_4.2.2_Ubuntu_22.04-13.23-09.prod.bfb
	```

4. Install DOCA v2.2.1 on DPU. Replace `rshim0` based on Step 2.

	Wait untill `Installation finished` message.
	
	```sh
	sudo bfb-install --rshim rshim0 --bfb DOCA_2.2.1_BSP_4.2.2_Ubuntu_22.04-13.23-09.prod.bfb
	```

5. Wait about 5 minutes for DPU to reboot.

	Monitor the virtual console looking for `DPU is ready` message:

	```sh
	sudo cat /dev/rshim0/misc
	```

6. Restart host-side DPU drivers:

	```sh
	sudo /etc/init.d/openibd restart
	```

7. Assign IPs to Host-side virtual port for DPU:

	```sh
	sudo ip addr add dev tmfifo_net0 192.168.100.1/30
	
	sudo ip link set tmfifo_net0 up
	```

8. To forward internet access from Host server to DPU do the following:

 - **On the Host:**

	a. Find the default gateway for Host server. On our server it was `eno12399np0`.
	
	```sh
	ip route | grep default
	```

	b, Set up forwarding tables. Replace `eno12399np0` with your interface based on the previous step.
	```sh
	sudo iptables -t nat -A POSTROUTING -o eno12399np0 -j MASQUERADE
	
	sudo iptables -A FORWARD -i tmfifo_net0 -j ACCEPT
	
	sudo iptables -A FORWARD -o tmfifo_net0 -j ACCEPT
	```

 - **On the DPU:**
	
	a. SSH into DPU first from host machine:
	```sh
	ssh ubuntu@192.168.100.2
	```

	b. Assign IPs to DPU-side virtual port for Host:
	```sh
	sudo /etc/init.d/openibd restart

	sudo ip link set tmfifo_net0 up

	sudo ip addr add 192.168.100.2/24 dev tmfifo_net0
	```

	c. Add default gateway and DNS:

	```sh
	sudo ip route add default via 192.168.100.1

	echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
	```

## C. Enabling RXP engine<a name="c"></a>

- **On the DPU:**

	```sh
	sudo mst start

	sudo systemctl start mlx-regex
	```

- **On the Host:**

	1. Stop host side drivers:

		```sh
		sudo /etc/init.d/openibd stop
		```

	2. Set up huge pages.
	
		We are setting up 1024 huge pages of size 2 MB.

		```sh
		echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
		
		sudo cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

		sudo /etc/init.d/openibd restart
		```

	3. Get PCI device ID for the DPU. Look for `domain:bus:dev.fn=<ID here>`. In my case it was: `0000:0d:00.0`
	
		```sh
		sudo mst start

		sudo mst status
		```

	3. Enable RXP physical function. Replace `0000:0d:00.0` with your DPU PCI ID.

		```sh
		echo 1 | sudo tee /sys/bus/pci/devices/0000:0d:00.0/regex/pf/regex_en

		sudo cat /sys/bus/pci/devices/0000:0d:00.0/regex/pf/regex_en
		```

## D. Compiling regular expressions<a name="d"></a>

**Do the following on the DPU-side.** The Host side will not have the RXPC compiler as explained earlier...

1. Setup DOCA paths in your environment:

	```sh
	ssh ubuntu@192.168.100.2

	export PATH=$PATH:/opt/mellanox/doca/tools/:/opt/mellanox/grpc/bin
	
	export PKG_CONFIG_PATH=/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig
	```

2. Delete old build folder (if it exists) and make a new one:

	```sh
	rm -rf build_regexes

	mkdir build_regexes
	```

3. A demo set of regular expressions are provided in `my_regexes.txt` in this repo to get started.

	```
	ca*t
	c[0-9]a+d
	pu+rdu+e
	((0[1-9]|[12]\d|3[01])-(0[1-9]|1[0-2])-[12]\d{3})
	```

4. Invoke RXPC compiler:

	```sh
	rxpc -V bf3 -f my_regexes.txt -o build_regexes/build_regexes
	```

## E. Build and run the sample RXP matcher program<a name="e"></a>

1. I am using a slightly modified version of the `meson` build script from NVIDA code. This will build the `simple_regex_scan.c` application.

	```sh
	rm -rf build

	meson build

	ninja -C build
	```

2. Get PCI device ID for the DPU. Look for `domain:bus:dev.fn=<ID here>`. In my case it was: `0000:0d:00.0`
	
	```sh
	sudo mst start

	sudo mst status
	```

3. Run the regex scan application.
	
	Replace `0000:0d:00.0` based on previous stepp.
	
	Replace `data.txt` with a file containing the data you want to scan against the regular expressions in `my_regexes.txt`.

	```sh
	sudo build/simple_regex_scan -p 0000:0d:00.0 -r build_regexes/build_regexes.rof2.binary -d data.txt
	```

4. You should see some some matches printed out on the console!

	```
	Matched regex 2 against data: num_car:5
	Matched regex 1 against data: ct
	Matched regex 1 against data: caaaat
	Matched regex 3 against data: purdue
	Matched regex 3 against data: puuuuuurdue
	```