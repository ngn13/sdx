import gdb


class BreakLower(gdb.Command):
    def __init__(self):
        super(BreakLower, self).__init__("blower", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, sym, from_tty):
        if sym is None or sym == "":
            return print("please specify a symbol or an address")

        cmd = "b *((%s)-0xffffffff80000000)" % sym
        gdb.execute(cmd, from_tty)

    def complete(self, text, word):
        return gdb.COMPLETE_SYMBOL


BreakLower()
