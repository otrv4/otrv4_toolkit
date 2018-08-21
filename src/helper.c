#include <stdio.h>

#include <goldilocks/point_448.h>

#include <libotr-ng/constants.h>
#include <libotr-ng/otrng.h>
#include <libotr-ng/shake.h>
#include <libotr/b64.h>

#include "parse.h"
#include "helper.h"

void print_string(char *data, int data_len) {
  for (int i = 0; i < data_len; i++) {
    printf("%c", data[i]);
  }
  printf("\n");
}

void print_plaintext_formated(char *data, int data_len) {
  static const char tag_base[] = {
      '\x20', '\x09', '\x20', '\x20', '\x09', '\x09', '\x09', '\x09', '\x20',
      '\x09', '\x20', '\x09', '\x20', '\x09', '\x20', '\x20', '\0'};

  size_t tag_length = WHITESPACE_TAG_BASE_BYTES + WHITESPACE_TAG_VERSION_BYTES;
  size_t chars = data_len - tag_length;
  char *found_at = strstr(data, tag_base);
  string_p buff = malloc(chars + 1);
  if (!buff) {
    // TODO: maybe this function should return a int to check status
    return;
  }

  size_t bytes_before_tag = found_at - data;
  if (!bytes_before_tag) {
    memcpy(buff, data + tag_length, chars);
  } else {
    memcpy(buff, data, bytes_before_tag);
    memcpy(buff, data + bytes_before_tag, chars - bytes_before_tag);
  }

  print_string(buff, strlen(buff));
}

void argv_to_buf(unsigned char **dst, size_t *written, char *arg) {
  unsigned char *buf;
  *dst = NULL;
  *written = 0;
  size_t size = strlen(arg);

  if (size % 2) {
    fprintf(stderr, "Argument ``%s'' must have even length.\n", arg);
    exit(1);
  }

  buf = malloc(size / 2);
  if (buf == NULL) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }

  char *b = malloc(3);
  if (b == NULL) {
    fprintf(stderr, "Out of memory!\n");
    free(buf);
    exit(1);
  }

  char *end;
  for (int i = 0; i < size / 2; i++) {
    strncpy(b, arg, 2);
    b[2] = 0;
    arg += 2;
    buf[i] = (int)strtol(b, &end, 16);
    if (*end) {
      free(b);
      free(buf);
      fprintf(stderr, "Error when trying to convert key!\n");
      exit(1);
    }
  }

  *dst = buf;
  *written = size / 2;

  free(b);
}

int decrypt_data_message(uint8_t *plain, const msg_enc_key_p enc_key,
                         const data_message_s *msg) {

  int err = crypto_stream_xor(plain, msg->enc_msg, msg->enc_msg_len, msg->nonce,
                              enc_key);

  if (err) {
    fprintf(stderr, "Error on decrypt!\n");
    return 1;
  }
  return 0;
}

int encrypt_data_message(data_message_s *data_msg, const char *msg,
                         size_t msg_len, const msg_enc_key_p enc_key) {

  uint8_t *enc_msg = malloc(msg_len);

  int err = crypto_stream_xor(enc_msg, (uint8_t *)msg, msg_len, data_msg->nonce,
                              enc_key);

  if (err) {
    fprintf(stderr, "Error on encrypt!\n");
    free(enc_msg);
    return 1;
  }

  data_msg->enc_msg_len = msg_len;
  data_msg->enc_msg = enc_msg;

  return 0;
}

void serialize_and_remac(char **encoded_data_msg, data_message_s *data_msg,
                         uint8_t *new_mac) {

  uint8_t *msg = NULL;
  size_t msg_len = 0;

  otrng_data_message_body_asprintf(&msg, &msg_len, data_msg);

  size_t encoded_data_msg_len = msg_len + DATA_MSG_MAC_BYTES;

  uint8_t *enc_msg = malloc(encoded_data_msg_len);
  if (!enc_msg) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }

  memcpy(enc_msg, msg, msg_len);
  free(msg);

  otrng_data_message_authenticator(enc_msg + msg_len, DATA_MSG_MAC_BYTES,
                                   new_mac, enc_msg, msg_len);

  *encoded_data_msg =
      otrl_base64_otr_encode(enc_msg, msg_len + DATA_MSG_MAC_BYTES);

  free(enc_msg);
}

void calculate_mac(msg_mac_key_p mac_key, unsigned char *buff) {

  memset(mac_key, 0, sizeof(msg_mac_key_p));

  shake_256_kdf1(mac_key, sizeof(msg_mac_key_p), usage_mac_key, buff,
                 sizeof(msg_enc_key_p));
}

// int modify_message(data_message_s *data_msg, char *original_msg,
//                   char *old_msg_txt, char *new_msg_txt, int offset) {
//
//  int result = decode_data_message(data_msg, original_msg);
//
//  if (result != 0) {
//    fprintf(stderr, "Error decoding data message");
//    otrng_data_message_free(data_msg);
//    return result;
//  }
//
//  for (int i = 0; i < strlen(old_msg_txt) && offset + i <
//  data_msg->enc_msg_len;
//       ++i) {
//    data_msg->enc_msg[offset + i] ^= (old_msg_txt[i] ^ new_msg_txt[i]);
//  }
//
//  return result;
//}
