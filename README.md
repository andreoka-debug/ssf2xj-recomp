# Super Street Fighter II X — recomp para PC (protótipo)

Porte experimental de **Super Street Fighter II X (Japan 940223)** para PC, montado sobre o toolkit [cps2recomp](https://github.com/tmcleroy/cps2recomp). Este repositório contém somente o código específico do porte, patches do toolkit, pontos de entrada para a tradução e filmes de teste. **Não contém ROMs, chave, EEPROM, programa traduzido, executável ou arquivos do CPS2X Web.** Cada pessoa compila localmente usando seus próprios conjuntos `ssf2xj.zip` e `ssf2t.zip`.

O foco atual é Windows. O protótipo oferece jogo local, filtros CRT e curvatura, modo Wide para os cenários e as telas de seleção, VS e resultado, e um modo de treino com seleção do P2, vida e medidor, histórico de inputs, dano, hitboxes, leitura de frames e gravação/reprodução do dummy em três slots. A tela inicial permanece em 4:3. O modo Wide expande a área renderizada sem alterar a câmera nem esticar os lutadores; algumas bordas dos cenários ainda têm limitações. Os números de frames são medidos por quadro apresentado e ainda precisam de validação em todas as velocidades.

## Créditos e relação com o projeto original

O toolkit [cps2recomp](https://github.com/tmcleroy/cps2recomp), de **Tommy McLeroy**, faz a tradução estática e fornece a execução CPS-2, o áudio, a interface e os componentes compartilhados do treino. O projeto [Vampire Savior Recomp](https://github.com/tmcleroy/vampire-savior-recomp) do mesmo autor foi a referência para o fluxo do modo de treino e seus recursos. O visualizador de hitboxes de [pof/ssf2xj](https://github.com/poliva/ssf2xj/blob/master/lua/pof-ssf2x-training2020.lua), baseado no trabalho de Dammit, MZ e Felineki, orientou a leitura das tabelas de colisão do SSF2X.

Este é um **porte complementar**, não uma cópia completa nem uma versão oficial dos projetos acima. `ssf2xj/src/` contém a integração específica do SSF2X; os arquivos `.patch` contêm as alterações necessárias no toolkit. O código é distribuído sob GPL-3.0, compatível com o [toolkit original](https://github.com/tmcleroy/cps2recomp/blob/main/LICENSE). Capcom e os demais titulares mantêm os direitos sobre o jogo e seus dados.

## Compilar no Windows

Instale Git, PowerShell 7 ou Windows PowerShell, e [MSYS2 UCRT64](https://www.msys2.org/) com as dependências de compilação do `cps2recomp` (CMake, Ninja, Python, compilador C/C++, Make e SDL3). Coloque `ssf2xj.zip` e `ssf2t.zip` na mesma pasta. Depois execute, na raiz deste repositório:

```powershell
./build.ps1 -RomPath 'C:\caminho\das\roms\ssf2xj.zip'
```

O script baixa o `cps2recomp` no commit `fdd55ecbd20c9197c85ef526ecebd44879ced82e`, aplica os patches, copia os fontes e os pontos de entrada e compila o jogo e seu launcher. Tudo o que for gerado fica em `_build/`, ignorado pelo Git. Na primeira compilação, a tradução a partir das ROMs pode demorar. O script também aceita `-MsysRoot` se o MSYS2 estiver em outro local.

Para jogar, abra `play.cmd` sem argumentos. O launcher permite escolher as ROMs e configurar os controles antes de iniciar. Se os dois ZIPs estiverem em `roms/`, a ROM principal já aparece selecionada. Para iniciar diretamente sem o launcher, passe o caminho da ROM:

```powershell
./play.ps1 -RomPath 'C:\caminho\das\roms\ssf2xj.zip'
```

As preferências e o EEPROM criado localmente ficam em `userdata/`, também ignorado pelo Git. O menu do jogo abre com `Esc` ou `F1`. Em **Display → Interface language**, escolha inglês, português, espanhol, japonês ou francês; a preferência fica salva. Essa opção traduz o menu do porte para PC; o texto original do arcade permanece como foi produzido pelo jogo. O japonês usa uma fonte do Windows, sem incluir arquivos de fonte no repositório. Em **Display → Wall helper**, ligue as linhas que marcam as bordas da área jogável original quando estiver em Wide. Em **Alternative modes → Training mode**, escolha o personagem do P2 e clique em **Start**; use **Hitboxes**, **Frame data** e **Frame meter** conforme desejar. Na seção **Dummy**, selecione o slot, ative **Record** (P1 passa a controlar P2) e depois **Replay**. **Start over** reinicia o round sem apagar as gravações.

## Testes e integração

Com as ROMs locais, o porte foi compilado do zero no Windows e passou nas verificações de equivalência entre código traduzido e interpretador e de repetibilidade do `cps2recomp`. Um teste automatizado confirmou a troca de REC para PLAY e a reprodução idêntica após salvar e carregar o estado. Ainda faltam testes manuais abrangentes para cada cenário e golpe.

Para integrar este porte ao toolkit, aplique os três arquivos `.patch` na ordem usada por `build.ps1` sobre o commit indicado, copie `ssf2xj/` para `games/ssf2xj/` e `entries/entries.txt*` para `work/ssf2xj/`. Os pontos de entrada são endereços, não bytes da ROM. `build.ps1` automatiza exatamente esses passos.
