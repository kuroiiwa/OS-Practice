# f18-hmwk6-team18
## PtreeFS
### Usage
#### Compile and Install
* `cd kernel`
* `cp vm.config .config`
* `make -j2`
* `sudo make modules_install && sudo make install`
#### Mount
* `cd /`
* `sudo mkdir ptreefs`
* `sudo mount -t ptreefs ptreefs /ptreefs`
#### Check Processes Tree
* `cd /`
* `cd /ptreefs`
* `ls -R`
#### Test
* `cd user`
* `make`
* `./ptreeps`
### Ptreeps Test File
* The `ptreeps` test file will give the current process hierarchy by calling `ls -R`. 
* It will fork 20 child processes. Each of them will wait 5s and the test file will display process tree.
* After its children exit, it will display tree again.
#### Result
`...`
```
======================================
After creating processes, name: ptreeps.name
======================================
.:
0

./0:
1  2  swapper-0.name
```
`...`
```
./0/1/2040/2583/2589/2718:
2720  2722  2724  2726	2728  2730  2732  2734	2736  2738  2740
2721  2723  2725  2727	2729  2731  2733  2735	2737  2739  ptreeps.name

./0/1/2040/2583/2589/2718/2720:
ptreeps.name

./0/1/2040/2583/2589/2718/2721:
ptreeps.name

./0/1/2040/2583/2589/2718/2722:
ptreeps.name
```
`...`


