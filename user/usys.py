def entry (name):
    print(".global " + name)
    print(name + ":")
    print("li a7, SYS_" + name)
    print("ecall")
    print("ret\n")

print("# 由usys.py自动生成 请勿修改")
print("#include \"includes/syscall.h\"\n")
entry("fork")
entry("exit")
entry("wait")
entry("pipe")
entry("read")
entry("write")
entry("close")
entry("kill")
entry("exec")
entry("open")
entry("mknod")
entry("unlink")
entry("fstat")
entry("link")
entry("mkdir")
entry("chdir")
entry("dup")
entry("getpid")
entry("sbrk")
entry("sleep")
entry("uptime")
