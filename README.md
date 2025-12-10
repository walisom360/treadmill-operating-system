Instruções de Compilação e Execução

## Compilação

Para compilar o projeto, abra um terminal na pasta onde estão os arquivos e execute:

Linux / macOS:
make

Windows (MinGW):
mingw32-make

Após a compilação, o executável será gerado com o nome:
linha_bolacha
ou, no Windows:
linha_bolacha.exe

## Execução

Linux / macOS:
./linha_bolacha

Windows:
linha_bolacha.exe

## Observações

- O arquivo config.txt é opcional. Se não existir, o sistema utiliza valores padrão.
- O arquivo log.txt será criado automaticamente durante a execução.
- Comandos disponíveis no terminal:
  s → Mostrar estado do sistema
  p → Pausar a linha de produção
  r → Retomar a linha de produção
  q → Encerrar o sistema
