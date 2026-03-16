/*
 * finger.h - Finger protocol (RFC 1288) client for Flynn
 */

#ifndef FINGER_H
#define FINGER_H

/* Show the Finger dialog and connect */
void do_finger(void);

/* Finger via bookmark (pre-filled host/user) */
void do_finger_bookmark(short bm_idx);

#endif /* FINGER_H */
