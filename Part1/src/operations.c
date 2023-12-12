#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "constants.h"
#include "eventlist.h"
#include "utils.h"
#include "operations.h"

pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;


/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
struct Event *get_event_with_delay(Ems_t *ems, unsigned int event_id) {
  struct timespec delay = delay_to_timespec(ems->state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return get_event(ems->event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
unsigned int *get_seat_with_delay(Ems_t *ems, struct Event *event, size_t index) {
  struct timespec delay = delay_to_timespec(ems->state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
size_t seat_index(struct Event *event, size_t row, size_t col) {
  return (row - 1) * event->cols + col - 1;
}

int ems_init(Ems_t *ems, unsigned int delay_ms) {
  if (ems->event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  ems->event_list = create_list();
  ems->state_access_delay_ms = delay_ms;

  return ems->event_list == NULL;
}

int ems_terminate(Ems_t *ems) {
  if (ems->event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(ems->event_list);
  return 0;
}

int ems_create(Ems_t *ems, unsigned int event_id, size_t num_rows, size_t num_cols) {
  pthread_rwlock_wrlock(&lock);
  if (ems->event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  if (get_event_with_delay(ems, event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  struct Event *event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");

    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(ems->event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    pthread_rwlock_unlock(&lock);
    return 1;
  }
  pthread_rwlock_unlock(&lock);
  return 0;
}

int ems_reserve(Ems_t *ems, unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys) {
  pthread_rwlock_wrlock(&lock);
  if (ems->event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  struct Event *event = get_event_with_delay(ems, event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(ems, event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(ems, event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(ems, event, seat_index(event, xs[j], ys[j])) = 0;
    }
    pthread_rwlock_unlock(&lock);
    return 1;
  }
  pthread_rwlock_unlock(&lock);
  return 0;
}

int ems_show(Ems_t *ems, unsigned int event_id, int out_fd) {
  pthread_rwlock_rdlock(&lock);
  if (ems->event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  struct Event *event = get_event_with_delay(ems, event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  char buffer[BUFSIZ];
  size_t n_bytes = 0;
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int *seat = get_seat_with_delay(ems, event, seat_index(event, i, j));
      ssize_t added_bytes = snprintf(buffer + n_bytes, BUFSIZ - n_bytes, "%u", *seat);

      if (added_bytes < 0) {
        fprintf(stderr, "Encoding error: could not add data to buffer\n");
        pthread_rwlock_unlock(&lock);
        return 1;
      }

      n_bytes += (size_t) added_bytes;
      if (j < event->cols) {
        buffer[n_bytes++] = ' ';
      }

      if (BUFSIZ - n_bytes <= BUFFER_FLUSH_THOLD) {
        if (utilwrite(out_fd, buffer, n_bytes)) {
          pthread_rwlock_unlock(&lock);
          return 1;
        }
        n_bytes = 0;
      }
    }
    buffer[n_bytes++] = '\n';
  }

  if (utilwrite(out_fd, buffer, n_bytes)) {
    pthread_rwlock_unlock(&lock);
    return 1;
  }
  pthread_rwlock_unlock(&lock);
  return 0;
}

int ems_list_events(Ems_t *ems, int out_fd) {
  pthread_rwlock_rdlock(&lock);
  if (ems->event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&lock);
    return 1;
  }

  char buffer[BUFSIZ];
  size_t n_bytes = 0;
  if (ems->event_list->head == NULL) {
    strcpy(buffer, "No events\n");
    n_bytes = strlen("No events\n");
  }
  else {
    struct ListNode *current = ems->event_list->head;
    while (current != NULL) {
      ssize_t added_bytes = snprintf(buffer + n_bytes, BUFSIZ - n_bytes, "Event: %u\n", (current->event)->id);
      if (added_bytes < 0) {
        fprintf(stderr, "Encoding error: could not add data to buffer\n");
        pthread_rwlock_unlock(&lock);
        return 1;
      }
      n_bytes += (size_t) added_bytes;

      if (BUFSIZ - n_bytes <= BUFFER_FLUSH_THOLD) {
        if (utilwrite(out_fd, buffer, n_bytes)) {
          pthread_rwlock_unlock(&lock);
          return 1;
        }
        n_bytes = 0;
      }
      current = current->next;
    }
    if (utilwrite(out_fd, buffer, n_bytes)) {
      pthread_rwlock_unlock(&lock);
      return 1;
    }
  }
  pthread_rwlock_unlock(&lock);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
