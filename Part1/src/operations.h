#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include <stddef.h>
#include "eventlist.h"

typedef struct Ems {
    struct EventList *event_list;
    unsigned int state_access_delay_ms;
} Ems_t;

/// Initializes the EMS state.
/// @param ems The EMS data structure
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(Ems_t *ems, unsigned int delay_ms);

/// Destroys the EMS state.
int ems_terminate(Ems_t *ems);

/// Creates a new event with the given id and dimensions.
/// @param ems The EMS data structure
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(Ems_t *ems, unsigned int event_id, size_t num_rows, size_t num_cols);

/// Creates a new reservation for the given event.
/// @param ems The EMS data structure
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(Ems_t *ems, unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys);

/// Outputs the given event to the file descriptor.
/// @param ems The EMS data structure
/// @param event_id Id of the event to print.
/// @param out_fd Output file descriptor
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(Ems_t *ems, unsigned int event_id, int out_fd);

/// Prints all the events.
/// @param ems The EMS data structure
/// @param out_fd Output file descriptor
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(Ems_t *ems, int out_fd);


#endif // EMS_OPERATIONS_H
