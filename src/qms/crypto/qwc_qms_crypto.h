#ifndef QWERTYCOIN_QMS_CRYPTO_H
#define QWERTYCOIN_QMS_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qwc_qms_crypto_buffer {
  uint8_t *data;
  size_t len;
} qwc_qms_crypto_buffer;

uint32_t qwc_qms_crypto_abi_version(void);
void qwc_qms_crypto_buffer_free(qwc_qms_crypto_buffer buffer);
int32_t qwc_qms_crypto_engine_new(qwc_qms_crypto_buffer *output,
                                  qwc_qms_crypto_buffer *error);
int32_t qwc_qms_crypto_prepare_contact_package(
    const uint8_t *state, size_t state_len,
    const uint8_t *genesis, size_t genesis_len,
    qwc_qms_crypto_buffer *output, qwc_qms_crypto_buffer *error);
int32_t qwc_qms_crypto_prepare_import_contact(
    const uint8_t *state, size_t state_len,
    const uint8_t *local_invitation_id, size_t local_invitation_id_len,
    const uint8_t *remote_package, size_t remote_package_len,
    uint64_t now_unix_seconds,
    qwc_qms_crypto_buffer *output, qwc_qms_crypto_buffer *error);
int32_t qwc_qms_crypto_prepare_send_text(
    const uint8_t *state, size_t state_len,
    const uint8_t *contact_id, size_t contact_id_len,
    const uint8_t *text, size_t text_len,
    uint64_t now_unix_seconds,
    qwc_qms_crypto_buffer *output, qwc_qms_crypto_buffer *error);
int32_t qwc_qms_crypto_prepare_receive_text(
    const uint8_t *state, size_t state_len,
    const uint8_t *contact_id, size_t contact_id_len,
    uint8_t message_type,
    const uint8_t *ciphertext, size_t ciphertext_len,
    qwc_qms_crypto_buffer *output, qwc_qms_crypto_buffer *error);

#ifdef __cplusplus
}
#endif

#endif
