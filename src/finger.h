/*
 * finger.h - Finger protocol (RFC 1288) client for Flynn
 */

#ifndef FINGER_H
#define FINGER_H

/* Finger dialog resource ID and items */
#define DLOG_FINGER_ID      137
#define FINGER_OK           1
#define FINGER_CANCEL       2
#define FINGER_HOST_LABEL   3
#define FINGER_HOST_FIELD   4
#define FINGER_USER_LABEL   5
#define FINGER_USER_FIELD   6
#define FINGER_VERBOSE_CHK  7
#define FINGER_DEFAULT_BTN  8

/* Show the Finger dialog and connect */
void do_finger(void);

/* Finger via bookmark (pre-filled host/user) */
void do_finger_bookmark(short bm_idx);

#endif /* FINGER_H */
