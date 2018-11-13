#ifndef __HASH_H__
#define __HASH_H__

#include <stdio.h>

struct HashInfo {
  char *chave;
  void *valor;
};

typedef struct HashInfo HashInfo;

typedef void *HashTable;

struct HashTable_t {
  /**
   * Cria a HashTable. Use tamanho, de preferencia, numeros primos.
   */
  HashTable (*create)(int tamanho);

  /**
   * Destroi a HashTable this;
   */
  void (*destroy)(HashTable this, void (*destruir_item)(void *item), int destruir_chave);

  /**
   * Insere valor na hashtable de acordo com a chave
   */
  void (*insert)(HashTable this, HashInfo info);

  /**
   * Retorna verdadeiro caso exista um registro com a chave passada.
   */
  int (*exists)(HashTable this, const char chave[]);

  /**
   * Retorna o registro cuja chave é a passada.
  */
  HashInfo (*get)(HashTable this, const char chave[]);

  /**
   * Retorna o numero de casas preenchidas da HashTable
   */
  unsigned (*length)(HashTable this);

  /**
   * Retorna o total de casas existentes na HashTable
   */
  unsigned (*max_size)(HashTable this);

  /**
   * A chave nao pode existir mais
   */
  void (*remove)(HashTable this, const char chave[]);

  /**
   * Printa a tabela em fp de acordo com a funcao to_string
   */
  void (*print)(
    HashTable this, char *(*to_string)(void *valor), FILE *fp);

  /**
   * Mapeia uma funcao pela lista inteira
   */
  void (*map)(HashTable this, void *other, void (*map_function)(const void *valor, void *other));
};

extern const struct HashTable_t HashTable_t;

#endif /* __HASH_H__ */