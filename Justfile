
RUNNER :=  "valgrind --leak-check=full --show-leak-kinds=all"
# RUNNER := "gdb -args"

compile *args:
    ninja --quiet -C build {{args}}

simple *args: compile
    {{RUNNER}} build/simple 127.0.0.1 {{args}}

thread *args: compile
    {{RUNNER}} build/thread 127.0.0.1 {{args}}

thread_queue *args: compile
    {{RUNNER}} build/thread_queue 127.0.0.1 {{args}}

select *args: compile
    {{RUNNER}} build/select 127.0.0.1 {{args}}

gendb:
    just compile -t compdb > compile_commands.json

