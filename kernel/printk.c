/* =============================================================================
 * kernel/printk.c - Implémentation de printk (printf pour le noyau)
 * ============================================================================= */

#include "printk.h"
#include "tty.h"
#include "vga.h"
#include "string.h"

/* =============================================================================
 * vprintk - Traitement principal du format avec va_list
 *
 * Parcourt le format caractère par caractère:
 *   - Caractère ordinaire → écriture directe
 *   - '%' → début d'un spécificateur → lecture de l'argument suivant
 *
 * Retourne le nombre total de caractères affichés.
 * ============================================================================= */
int vprintk(const char *fmt, va_list ap)
{
    int    count = 0;   /* Nombre de caractères écrits */
    char   buf[34];     /* Buffer temporaire pour les conversions numériques */

    while (*fmt != '\0') {

        /* Caractère ordinaire: l'afficher directement */
        if (*fmt != '%') {
            tty_putchar(*fmt);
            count++;
            fmt++;
            continue;
        }

        /* On a un '%': regarder le spécificateur suivant */
        fmt++;  /* Sauter le '%' */

        /* Gérer les flags optionnels (on supporte '0' pour le padding) */
        int  zero_pad = 0;
        int  width    = 0;

        if (*fmt == '0') {
            zero_pad = 1;
            fmt++;
        }
        while (*fmt >= '1' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Identifier le spécificateur de conversion */
        switch (*fmt) {

            /* %c - Caractère unique */
            case 'c': {
                char c = (char)va_arg(ap, int);
                tty_putchar(c);
                count++;
                break;
            }

            /* %s - Chaîne de caractères */
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (s == NULL) s = "(null)";
                while (*s) {
                    tty_putchar(*s++);
                    count++;
                }
                break;
            }

            /* %d - Entier signé décimal */
            case 'd':
            case 'i': {
                int val = va_arg(ap, int);
                itoa(val, buf, 10);
                /* Padding avec des zéros ou des espaces */
                int len = (int)strlen(buf);
                while (len < width) {
                    tty_putchar(zero_pad ? '0' : ' ');
                    count++;
                    len++;
                }
                tty_puts(buf);
                count += (int)strlen(buf);
                break;
            }

            /* %u - Entier non signé décimal */
            case 'u': {
                uint32_t val = va_arg(ap, uint32_t);
                utoa(val, buf, 10);
                int len = (int)strlen(buf);
                while (len < width) {
                    tty_putchar(zero_pad ? '0' : ' ');
                    count++;
                    len++;
                }
                tty_puts(buf);
                count += (int)strlen(buf);
                break;
            }

            /* %x - Hexadécimal minuscules */
            case 'x': {
                uint32_t val = va_arg(ap, uint32_t);
                utoa(val, buf, 16);
                /* S'assurer que la chaîne est en minuscules (utoa les produit déjà) */
                int len = (int)strlen(buf);
                while (len < width) {
                    tty_putchar(zero_pad ? '0' : ' ');
                    count++;
                    len++;
                }
                tty_puts(buf);
                count += (int)strlen(buf);
                break;
            }

            /* %X - Hexadécimal majuscules */
            case 'X': {
                uint32_t val = va_arg(ap, uint32_t);
                utoa(val, buf, 16);
                /* Convertir en majuscules */
                for (int i = 0; buf[i]; i++) {
                    if (buf[i] >= 'a' && buf[i] <= 'f')
                        buf[i] = (char)(buf[i] - 32);
                }
                int len = (int)strlen(buf);
                while (len < width) {
                    tty_putchar(zero_pad ? '0' : ' ');
                    count++;
                    len++;
                }
                tty_puts(buf);
                count += (int)strlen(buf);
                break;
            }

            /* %p - Pointeur (affiché en hex avec préfixe 0x) */
            case 'p': {
                uint32_t val = va_arg(ap, uint32_t);
                tty_puts("0x");
                utoa(val, buf, 16);
                /* Padder à 8 chiffres pour un pointeur 32bits */
                int len = (int)strlen(buf);
                while (len < 8) {
                    tty_putchar('0');
                    count++;
                    len++;
                }
                tty_puts(buf);
                count += 2 + (int)strlen(buf);
                break;
            }

            /* %% - Littéral '%' */
            case '%':
                tty_putchar('%');
                count++;
                break;

            /* Spécificateur inconnu: afficher tel quel */
            default:
                tty_putchar('%');
                tty_putchar(*fmt);
                count += 2;
                break;
        }

        fmt++;
    }

    return count;
}

/* =============================================================================
 * printk - Version sans valeur de retour (la plus utilisée dans un noyau)
 * ============================================================================= */
void printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintk(fmt, ap);
    va_end(ap);
}

/* =============================================================================
 * kprintf - Version qui retourne le nombre de caractères écrits
 * ============================================================================= */
int kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vprintk(fmt, ap);
    va_end(ap);
    return n;
}
