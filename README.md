# Como compilar
É necessário ter instalado o build system Meson. Nas distribuições baseadas em Debian/Ubuntu, pasta executar `sudo apt install meson`.

E então, basta seguir o passo a passo:
1. `meson setup BUILD`
2. `meson compile BUILD`

Após isso, serão criados os executáveis `BUILD/{server,client}`.

# Como utilizar
- `client [IP] [PORT] <tcp|udp>`: Se conecta ao IP:PORT usando tcp ou udp, lê um arquivo e reporta estatísticas.

- `server [IP] [PORT] <tcp|udp> [FILENAME]`: Escuta ao IP:PORT usando tcp ou udp. Quando um cliente se conecta, envia os conteúdos do arquivo `FILENAME` ao cliente.

