# f18-hmwk2-team18

## Files modified
* /arch/x86/entry/syscalls/syscall_64.tbl
* /include/linux/syscalls.h
* /kernel/sys.c
* /syscall_test/test.c

## ptree system call

A same struct prinfo is created in the kernel using the number of size user expected. While traversing the process, we extract the data from task_struct and save it in the kernel prinfo struct. Only expected or actual amount of data (depends on which one is smaller) will be saved in the struct. Finally, we use copy_to_user to copy the kernel struct to user struct and free the kernel space. When user is listing the prinfo struct, it is responsible for user to not access the address space outside the prinfo struct he has defined. (We can't get the actual size of struct that user gives to kernel)

## Part 4
### a. Run your test program several times both using Android and Linux. Which fields in the prinfo structure change? Which ones do not? Discuss why different fields might change with different frequency.

Some of the PID fields (such as parent_pid, first_child_pid and next_sibling_pid) as well as state can change in the prinfo structure between different executions of the program. As processes are started or terminated, the parent-child and sibling relationships between processes will change, resulting in changes to the aforementioned PID fields. Additionally, the state of a process will change over its lifetime as its created, run or terminated. Since processes change state relatively frequently, we can expect this field to be updated more often than the PID fields.

Fields like the current process's PID, name and the process owner's UID are set upon creation and will not change over the course of the process's lifetime.

### b. How does the process tree for the VM compare to the Android process tree?

Both process trees contain a similar number of processes; however, the many of the processes listed under init in the Android process tree are mobile-specific (i.e. power, sensors, wifi, camera, fingerprint, etc.) and are not present in the Linux VM process tree.

### c. Notice that on the Android platform there is a process named zygote (This process may be called "main" in your process tree. Jot down the PID of "main" and run the ps command to investigate). Answer the following questions after digging through your process tree and ps:
#### i. What is the purpose of this process?

The zygote process is responsible for creating any and all application-level processes by forking itself whenever a new application is started. It also has core libraries linked in and these libraries are not copied to the child process when the zygote forks.

#### ii. Where is the zygote binary? If you can't find it, how might you explain its presence in your list of processes?

There is no zygote binary as the zygote process is created on system startup by the init.

#### iii. Discuss some reasons why an embedded system might choose to use a process like the zygote versus what happens with your Linux VM. HINT: A key feature of embedded systems is their resource limitations.

The zygote is a process that is created on system startup and has all core Android libraries loaded into it. Additionally, when the zygote forks itself, the core libraries are not copied to the new address space. Instead, the Linux copy on write policy is adopted, and data is only copied if the new process attempts to modify it.

In this way, a process like the zygote might be preferable for an embedded system, which has resource limitations, since it doesn't copy all of the data from the parent process to its child when it calls fork, thus reducing the amount of system memory consumed by processes.

References:
1. http://coltf.blogspot.com/p/android-os-processes-and-zygote.html
2. https://serializethoughts.com/2016/04/15/android-zygote/
