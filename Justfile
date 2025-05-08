
client *args:
    @just compile 2>&1
    build/client {{args}}

server *args:
    @just compile 2>&1
    build/server {{args}}

dbg-client *args:
    @just compile 2>&1
    gdb -args build/client {{args}}

dbg-server *args:
    @just compile 2>&1
    gdb -args build/server {{args}}

gendb:
    just compile -t compdb > compile_commands.json

compile *args:
    @ninja --quiet -C build {{args}}
