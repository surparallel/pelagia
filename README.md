# surparallel（pelagia） ![C Ubuntu](https://github.com/surparallel/pelagia/workflows/C%20Ubuntu/badge.svg) ![C Macos](https://github.com/surparallel/pelagia/workflows/C%20Macos/badge.svg)

## Introduce

Pelagia is an automatic parallelization tool (lock-free) developed by surparallel open source based on the concept of sequential virtual machine.
Pelagia is developed by ANSI C, abides by AGPL protocol, and supports multiple operating systems and languages.
It supports automatic parallelization, transparent thread calling, embedded key value database, embedded multi language running environment, and provides API and documents for each language.
The set concept in Boolean algebra is used to describe the operation of data. Including key, value, ordered set, index set and other types.

## Principle

Pelagia slices the attribute according to the ECS program. According to the property read and write different automatically allocated to different threads to improve the efficiency.Pelagia is very impressed with writing. All the table written from the order must be allow by "plg_MngAddTable".To use the following methods in order, you need to set write permissions. Otherwise, the write will fail.

```c
plg_JobSet
plg_JobMultiSet
plg_JobDel
plg_JobSetIfNoExit
plg_JobTableClear
plg_JobRename
```

```c
plg_JobSAdd
plg_JobSMove
plg_JobSPop
plg_JobSDel
plg_JobSUionStore
plg_JobSInterStore
plg_JobSDiffStore
```

## Related resources

Pelagia website: https://surparallel.org
Manual: 
<a href="https://surparallel.org/pelagia_c_api_manual_0.33.html">pelagia c api manual 0.33(html)</a> 
<a href="https://github.com/surparallel/pelagia/releases/download/release_src_0.31/pelagia_c_api_manual_0.33.pdf">pelagia c api manual 0.33(pdf)</a> 
<a href="https://surparallel.org/Pelagia_lua_0.33.html">pelagia lua api manual 0.33(html)</a> 
<a href="https://github.com/surparallel/pelagia/releases/download/release_src_0.31/Pelagia_lua_0.33.pdf">pelagia lua api manual 0.33(pdf)</a>

## Example

You can find the relevant sample code in psimple.c, profesa.c under the source code.

## Environmental installation

## Install on Linux system

Linux&Mac installation is very simple, just download the source package and decompress and compile it on the terminal. This article uses version 0.1 for installation:

    git clone https://github.com/surparallel/pelagia.git
    cd pelagia/src
    make linux
    
##  Install on Mac OS X

    git clone https://github.com/surparallel/pelagia.git
    cd pelagia/src
    make macosx
    
## Install on window system

After decompression, enter the msvcs directory and open pelagia.sln for compilation.

## Using Pelagia as a Control and Dao framework on the client

An example of unity can be found here
<a href="https://github.com/surparallel/unity_example_of_pelagia">https://github.com/surparallel/unity_example_of_pelagia</a>

Pelagia is data and logic management tool for clients. By separating view and control, software complexity is reduced, development efficiency and view performance are improved.
<a href="https://surparallel.org"><img src="https://github.com/surparallel/pelagia_websit/blob/master/img/view_control_dao.jpg"></a>


