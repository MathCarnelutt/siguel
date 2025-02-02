#include <comando.r>
#include <controlador.r>

#include <stdlib.h>
#include <string.h>

#include "svg_custom.h"

#include "../funcoes_checagem.h"

#include <model/estrutural/desenhavel.h>

#include <model/trabalho/elemento.h>
#include <model/trabalho/comercio.h>
#include <model/trabalho/endereco.h>
#include <model/trabalho/pessoa.h>
#include <model/trabalho/figura.h>

#include <utils/utils.h>

#define NUCLEAR_SVG \
"<svg version=\"1.1\" id=\"Capa_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"" \
"	viewBox=\"0 0 512 512\" style=\"opacity:0.8;enable-background:new 0 0 512 512;\" xml:space=\"preserve\">" \
"<polygon style=\"fill:#EB874B;\" points=\"512,245.556 371.069,208.064 442.546,82.405 310.181,143.136 283.136,1.508 221.273,132.046 " \
"	108.361,40.717 145.946,179.982 0,181.687 119.446,264.514 8.756,358.455 154.173,346.089 130.531,488.311 233.878,386.538 " \
"	308.347,510.492 321.266,366.933 459.001,414.621 375.446,296.448 \"/>" \
"<polygon style=\"fill:#FFC850;\" points=\"421.161,249.262 330.238,225.073 376.352,144.003 290.956,183.185 273.507,91.811 " \
"	233.595,176.029 160.749,117.108 184.997,206.956 90.839,208.056 167.901,261.493 96.488,322.1 190.305,314.122 175.052,405.878 " \
"	241.728,340.218 289.772,420.189 298.107,327.57 386.968,358.336 333.062,282.096 \"/>" \
"<polygon style=\"fill:#FFDC64;\" points=\"357.161,251.873 301.471,237.058 329.716,187.402 277.41,211.401 266.723,155.434 " \
"	242.277,207.018 197.659,170.929 212.511,225.961 154.839,226.634 202.039,259.364 158.299,296.486 215.762,291.6 206.42,347.8 " \
"	247.258,307.584 276.685,356.566 281.79,299.836 336.218,318.681 303.2,271.984 \"/>" \
"<polygon style=\"fill:#FFF082;\" points=\"296.258,254.358 274.095,248.462 285.336,228.701 264.52,238.251 260.267,215.979 " \
"	250.539,236.507 232.783,222.145 238.693,244.046 215.742,244.314 234.526,257.339 217.119,272.112 239.987,270.167 " \
"	236.269,292.533 252.521,276.528 264.232,296.021 266.264,273.445 287.923,280.944 274.784,262.361 \"/>" \
"</svg>"

static int __dentro_figura(void * quadra, void *figura) {
  Figura fig_quadra = get_figura_elemento(quadra);

  int result = dentro_figura((Figura) figura, fig_quadra);

  destruir_figura(fig_quadra);

  return !result;
}

static int __comercioDentro(void *comercio, void *cep_quadra) {
  int result = strcmp(comercio_get_endereco(comercio)->cep, cep_quadra);

  if (result != 0)
    comercio_destruir(comercio);

  return result;
}

static Lista_t __get_elementos_dentro(KDTree_t arvore, Ponto2D_t pos, Ponto2D_t size, enum TipoElemento tipo) {
  Ponto2D_t pA = pos;
  Ponto2D_t pB = p2d_add(pos, size);

  Lista_t saida = kdt_range_search(arvore, elemento_dentro_rect, &pA, &pB);

  if (tipo == QUADRA) {
    Figura figura = cria_retangulo(pos.x, pos.y, size.x, size.y, "", "");

    Lista_t lista_aux = lt_filter(saida, figura, __dentro_figura);

    lt_destroy(saida, 0);

    saida = lista_aux;

    destruir_figura(figura);
  }
  

  if (lt_length(saida) == 0) {
    lt_destroy(saida, 0);
    return NULL;
  }

  return saida;
}

static void __adicionarHashComercio(void * comercio, void *tabela) {
  ht_insert((HashTable_t) tabela, comercio_get_cnpj(comercio), comercio);
}

static int __pessoaDentro(void * pessoa, void *cep_quadra) {
  if (!pessoa_get_cep(pessoa)) return 1;
  return strcmp((char *) pessoa_get_cep(pessoa), cep_quadra);
}

static void __removerEnderecoPessoa(void * pessoa, void *tabela) {
  pessoa_remover_endereco(pessoa);
  ht_remove((HashTable_t) tabela, pessoa_get_cpf(pessoa));
}


static void __remover_elementos(
  struct Comando *this,
  struct Controlador *controlador,
  Lista_t *elementos) {

  for (int h = 0; h < 4; h++) {
    if (!elementos[h]) continue;

    const char *tipo_elemento = get_tipo_string_elemento(h);

    Posic_t it = lt_get_first(elementos[h]);
    while (it) {
      Elemento elemento = lt_get(elementos[h], it);

      kdt_remove(controlador->elementos[h], elemento);
      char *cep   = get_cep_elemento(elemento);
      char *saida = format_string("\t%s: %s deletado (a).\n", tipo_elemento, cep);
      lt_insert(controlador->saida, saida);

      // destruir_elemento(elemento);

      it = lt_get_next(elementos[h], it);
    }

    lt_destroy(elementos[h], destruir_elemento);

  }

}

/**
 * Comando: dpr
 * Params:  x y larg alt
 * Desapropria região.
 * Apagar quadras, hidrantes, etc dentro da região;
 * etc A pessoa não morre, mas deixa de ser morador.
 * SAIDA: arquivo .txt => listar o que foi removido
 * arquivo .svg => elementos removidos não devem
 * aparecer.
 */
int comando_qry_dpr(void *_this, void *_controlador) {
  struct Comando     *this        = _this;
  struct Controlador *controlador = _controlador;

  Ponto2D_t pos  = p2d_new(strtod(this->params[0], 0), strtod(this->params[1], 0));
  Ponto2D_t size = p2d_new(strtod(this->params[2], 0), strtod(this->params[3], 0));

  Lista_t elementos_dentro[4];

  for (int i = 0; i < 4; i++) {
    KDTree_t arvore = controlador->elementos[i];
    elementos_dentro[i] = __get_elementos_dentro(arvore, pos, size, i);
  }

  lt_insert(controlador->saida, 
    format_string("Comando DPR:\n"));


  if (elementos_dentro[QUADRA]) {

    Posic_t it = lt_get_first(elementos_dentro[QUADRA]);
    while (it) {
      Elemento quadra = lt_get(elementos_dentro[QUADRA], it);

      // Retirar das pessoas os endereços das quadras destruidas e remover da tabela CPF/CEP
      Lista_t pessoas_dentro = lt_filter(controlador->pessoas, get_cep_elemento(quadra), __pessoaDentro);
      lt_map(pessoas_dentro, controlador->tabelas[CPF_X_CEP], __removerEnderecoPessoa);
      lt_destroy(pessoas_dentro, 0);

      // Destruir os comercios nas quadras e tirar da tabela CNPJ/COMERCIO
      Lista_t new_comercios = lt_filter(controlador->comercios, get_cep_elemento(quadra), __comercioDentro);
      lt_destroy(controlador->comercios, 0);
      ht_destroy(controlador->tabelas[CNPJ_X_COMERCIO], NULL, 0);

      controlador->comercios = new_comercios;
      controlador->tabelas[CNPJ_X_COMERCIO] = ht_create(lt_length(new_comercios));

      lt_map(new_comercios, controlador->tabelas[CNPJ_X_COMERCIO], __adicionarHashComercio);

      // Remover da tabela CEP/QUADRA as quadras removidas
      ht_remove(controlador->tabelas[CEP_X_QUADRA], get_cep_elemento(quadra));

      it = lt_get_next(elementos_dentro[QUADRA], it);
    }
  }

  // Remover da tabela ID/RADIO_BASE as radios removidas
  if (elementos_dentro[RADIO_BASE]) {
    Posic_t it = lt_get_first(elementos_dentro[RADIO_BASE]);
    while (it) {
      Elemento radio_base = lt_get(elementos_dentro[RADIO_BASE], it);
      ht_remove(controlador->tabelas[ID_X_RADIO], get_id_elemento(radio_base));
      it = lt_get_next(elementos_dentro[RADIO_BASE], it);
    }
  }

  // Remover todos os equipamentos em elementos_dentro
  __remover_elementos(this, controlador, elementos_dentro);
  
  double tamanho = min(size.x, size.y) * 0.8;

  Figura area_afetada = cria_retangulo(pos.x, pos.y, size.x, size.y, "transparent", "teal");
  set_dashed_figura(area_afetada, FIG_BORDA_TRACEJADA);
  
  Ponto2D_t posicao_asset = p2d_new(0, 0);
  posicao_asset = p2d_sub_scalar(posicao_asset, tamanho / 2);

  // posicao_asset agora marca o centro do local
  // Agora é só adicionar a posicao do centro do retangulo para posicao_asset
  posicao_asset = p2d_add(posicao_asset, get_centro_massa(area_afetada));

  void *custom = cria_custom(posicao_asset, tamanho, NUCLEAR_SVG, NULL);

  lt_insert(controlador->saida_svg_qry, cria_desenhavel(
    custom, print_custom_asset, free_custom));

  lt_insert(controlador->saida_svg_qry, cria_desenhavel(
    area_afetada, get_svg_figura, destruir_figura));

  return 1;
}