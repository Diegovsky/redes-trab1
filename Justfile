
compile *args:
    ninja --quiet -C build {{args}}

simple *args: compile
    build/simple 127.0.0.1 {{args}}

thread *args: compile
    build/thread 127.0.0.1 {{args}}

gendb:
    just compile -t compdb > compile_commands.json

