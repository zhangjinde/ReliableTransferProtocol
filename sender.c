#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

/* Flag set by ‘--verbose’. */
static int verbose_flag = 0;

int main (int argc, char **argv) {

  //     _    ____   ____ ____
  //    / \  |  _ \ / ___/ ___|
  //   / _ \ | |_) | |  _\___ \
  //  / ___ \|  _ <| |_| |___) |
  // /_/   \_\_| \_\\____|____/

  int c = -1;
  char *filename = NULL;
  int sber  = 0;
  int splr  = 0;
  int delay = 0;
  char *hostname = NULL;
  char *port = NULL;

  while (1) {
    static struct option long_options[] =
    {
      /* These options set a flag. */
      {"verbose", no_argument,       &verbose_flag, 1},
      {"brief"  , no_argument,       &verbose_flag, 0},
      /* These options don't set a flag.
         We distinguish them by their indices. */
      {"file",    required_argument, NULL, 'f'},
      {"sber",    required_argument, NULL, 'e'},
      {"splr",    required_argument, NULL, 'l'},
      {"delay",   required_argument, NULL, 'd'},
      {NULL, 0, NULL, 0}
    };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    char *endptr = NULL;
    switch (c) {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != NULL)
          break;
        printf ("option %s", long_options[option_index].name);
        if (optarg)
          printf (" with arg %s", optarg);
        printf ("\n");
        break;

      case 'f':
        filename = optarg;
        break;

      case 'e':
        sber = strtol(optarg, &endptr, 10);
        if (*optarg == '\0' || *endptr != '\0' || sber < 0 || sber > 1000) {
          fprintf(stderr, "Invalid sber\n");
          exit(1);
        }
        break;

      case 'l':
        splr = strtol(optarg, &endptr, 10);
        if (*optarg == '\0' || *endptr != '\0' || sber < 0 || sber > 1000) {
          fprintf(stderr, "Invalid splr\n");
          exit(1);
        }
        break;

      case 'd':
        delay = strtol(optarg, &endptr, 10);
        if (*optarg == '\0' || *endptr != '\0' || delay < 0) {
          fprintf(stderr, "Invalid delay\n");
          exit(1);
        }
        break;

      case '?':
        /* getopt_long already printed an error message. */
        break;

      default:
        abort();
    }
  }

  /* Instead of reporting ‘--verbose’
     and ‘--brief’ as they are encountered,
     we report the final status resulting from them. */
  if (verbose_flag)
    puts ("verbose flag is set");

  if (optind >= argc) {
    fprintf(stderr, "Missing hostname\n");
    exit(1);
    hostname = argv[optind++];
  } else {
    hostname = argv[optind++];
  }

  if (optind >= argc) {
    fprintf(stderr, "Missing port\n");
    exit(1);
  } else {
    port = argv[optind++];
  }

  if (optind < argc) {
    printf ("unused arguments: ");
    while (optind < argc)
      printf ("%s ", argv[optind++]);
    putchar ('\n');
  }


  //  ___ ___
  // |_ _/ _ \
  //  | | | | |
  //  | | |_| |
  // |___\___/


  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s, j;

  /* Obtain address(es) matching host/port */

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;
  hints.ai_protocol = IPPROTO_UDP;

  s = getaddrinfo(hostname, port, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully connect(2).
     If socket(2) (or connect(2)) fails, we (close the socket
     and) try the next address. */

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype,
        rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break;                  /* Success */

    close(sfd);
  }

  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not connect\n");
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(result);           /* No longer needed */

  /* Send remaining command-line arguments as separate
     datagrams, and read responses from server */

  int fd = get_fd(filename, false);

  size_t len = BUF_SIZE;
  ssize_t nread = 0;
  char buf[BUF_SIZE+1];

  while (len == BUF_SIZE) {
    len = read(fd, buf, len);
    if (len < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    if (write(sfd, buf, len) != len) {
      fprintf(stderr, "partial/failed write\n");
      exit(EXIT_FAILURE);
    }

    nread = read(sfd, buf, BUF_SIZE);
    if (nread == -1) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    buf[nread] = '\0';

    printf("Received %zd bytes: %s\n", nread, buf);
  }

  exit(EXIT_SUCCESS);
}
