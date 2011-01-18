#include <sys/param.h>
#include <errno.h>
#include <libutil.h>
#include <inttypes.h>

#include <libgeom.h>
#include <dialog.h>
#include <dlg_keys.h>

#include "partedit.h"

#define MIN_FREE_SPACE		(1024*1024*1024) /* 1 GB */
#define SWAP_SIZE(available)	MIN(available/20, 4*1024*1024*1024LL)

static char *boot_disk(struct gmesh *mesh);
static char *wizard_partition(struct gmesh *mesh, const char *disk);
static int wizard_makeparts(struct gmesh *mesh, const char *disk);

int
part_wizard(void) {
	int error;
	struct gmesh mesh;
	char *disk, *schemeroot;

startwizard:
	error = geom_gettree(&mesh);

	dlg_put_backtitle();
	error = geom_gettree(&mesh);
	disk = boot_disk(&mesh);
	if (disk == NULL)
		return (1);

	dlg_clear();
	dlg_put_backtitle();
	schemeroot = wizard_partition(&mesh, disk);
	free(disk);
	if (schemeroot == NULL)
		return (1);

	geom_deletetree(&mesh);
	dlg_clear();
	dlg_put_backtitle();
	error = geom_gettree(&mesh);

	error = wizard_makeparts(&mesh, schemeroot);
	if (error)
		goto startwizard;
	free(schemeroot);
	
	geom_deletetree(&mesh);

	return (0);
}

static char *
boot_disk(struct gmesh *mesh)
{
	struct gclass *classp;
	struct ggeom *gp;
	struct gprovider *pp;
	DIALOG_LISTITEM *disks = NULL;
	char diskdesc[512];
	char *chosen;
	int i, err, selected, n = 0;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "DISK") != 0 &&
		    strcmp(classp->lg_name, "MD") != 0)
			continue;

		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			if (LIST_EMPTY(&gp->lg_provider))
				continue;

			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
				disks = realloc(disks, (++n)*sizeof(disks[0]));
				disks[n-1].name = pp->lg_name;
				humanize_number(diskdesc, 7, pp->lg_mediasize,
				    "B", HN_AUTOSCALE, HN_DECIMAL);
				if (strncmp(pp->lg_name, "ad", 2) == 0)
					strcat(diskdesc, " ATA Hard Disk");
				else if (strncmp(pp->lg_name, "da", 2) == 0)
					strcat(diskdesc, " SCSI Hard Disk");
				else if (strncmp(pp->lg_name, "md", 2) == 0)
					strcat(diskdesc, " Memory Disk");
				else if (strncmp(pp->lg_name, "cd", 2) == 0) {
					n--;
					continue;
				}
				disks[n-1].text = strdup(diskdesc);
				disks[n-1].help = NULL;
				disks[n-1].state = 0;
			}
		}
	}

	if (n > 1) {
		err = dlg_menu("Partitioning",
		    "Select the disk on which to install FreeBSD.", 0, 0, 0,
		    n, disks, &selected, NULL);

		chosen = (err == 0) ? strdup(disks[selected].name) : NULL;
	} else if (n == 1) {
		chosen = strdup(disks[0].name);
	} else {
		chosen = NULL;
	}

	for (i = 0; i < n; i++)
		free(disks[i].text);

	return (chosen);
}

static struct gprovider *
provider_for_name(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;
	struct gprovider *pp = NULL;
	struct ggeom *gp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "DISK") != 0 &&
		    strcmp(classp->lg_name, "PART") != 0 &&
		    strcmp(classp->lg_name, "MD") != 0)
			continue;

		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			if (LIST_EMPTY(&gp->lg_provider))
				continue;

			LIST_FOREACH(pp, &gp->lg_provider, lg_provider)
				if (strcmp(pp->lg_name, name) == 0)
					break;

			if (pp != NULL) break;
		}

		if (pp != NULL) break;
	}

	return (pp);
}

static char *
wizard_partition(struct gmesh *mesh, const char *disk)
{
	struct gclass *classp;
	struct ggeom *gpart = NULL;
	struct gconfig *gc;
	char message[512];
	const char *scheme = NULL;
	char *retval = NULL;
	int choice;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class)
		if (strcmp(classp->lg_name, "PART") == 0)
			break;

	if (classp != NULL) {
		LIST_FOREACH(gpart, &classp->lg_geom, lg_geom) 
			if (strcmp(gpart->lg_name, disk) == 0)
				break;
	}

	if (gpart != NULL) {
		LIST_FOREACH(gc, &gpart->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "scheme") == 0) {
				scheme = gc->lg_val;
				break;
			}
		}
	}

query:
	dialog_vars.yes_label = "Entire Disk";
	dialog_vars.no_label = "Partition";
	if (gpart != NULL)
		dialog_vars.defaultno = TRUE;

	snprintf(message, sizeof(message), "Would you like to use this entire "
	    "disk (%s) for FreeBSD or partition it to share it with other "
	    "operating systems? Using the entire disk will erase any data "
	    "currently stored there.", disk);
	choice = dialog_yesno("Partition", message, 0, 0);

	dialog_vars.yes_label = NULL;
	dialog_vars.no_label = NULL;
	dialog_vars.defaultno = FALSE;

	if (scheme == NULL || strcmp(scheme, "(none)") == 0 || choice == 0) {
		if (gpart != NULL) { /* Erase partitioned disk */
			choice = dialog_yesno("Confirmation", "This will erase "
			   "the disk. Are you sure you want to proceed?", 0, 0);
			if (choice != 0)
				goto query;

			gpart_destroy(gpart, 1);
		}

		gpart_partition(disk, default_scheme());
		scheme = default_scheme();
	}

	if (strcmp(scheme, "PC98") == 0 || strcmp(scheme, "MBR") == 0) {
		struct gmesh submesh;
		geom_gettree(&submesh);
		gpart_create(provider_for_name(&submesh, disk),
		    "freebsd", NULL, NULL, &retval,
		    choice /* Non-interactive for "Entire Disk" */);
		geom_deletetree(&submesh);
	} else {
		retval = strdup(disk);
	}

	return (retval);
}

static int
wizard_makeparts(struct gmesh *mesh, const char *disk)
{
	struct gmesh submesh;
	struct gclass *classp;
	struct ggeom *gp;
	struct gconfig *gc;
	const char *scheme;
	struct gprovider *pp;
	intmax_t swapsize, available;
	char swapsizestr[10], rootsizestr[10];
	int retval;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class)
		if (strcmp(classp->lg_name, "PART") == 0)
			break;

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) 
		if (strcmp(gp->lg_name, disk) == 0)
			break;

	LIST_FOREACH(gc, &gp->lg_config, lg_config) 
		if (strcmp(gc->lg_name, "scheme") == 0) 
			scheme = gc->lg_val;

	pp = provider_for_name(mesh, disk);

	available = gpart_max_free(gp, NULL)*pp->lg_sectorsize;
	if (available < MIN_FREE_SPACE) {
		char availablestr[10], neededstr[10], message[512];
		humanize_number(availablestr, 7, available, "B", HN_AUTOSCALE,
		    HN_DECIMAL);
		humanize_number(neededstr, 7, MIN_FREE_SPACE, "B", HN_AUTOSCALE,
		    HN_DECIMAL);
		sprintf(message, "There is not enough free space on %s to "
		    "install FreeBSD (%s free, %s required). Would you like "
		    "to choose another disk or to open the partition editor?",
		    disk, availablestr, neededstr);

		dialog_vars.yes_label = "Another Disk";
		dialog_vars.no_label = "Editor";
		retval = dialog_yesno("Warning", message, 0, 0);
		dialog_vars.yes_label = NULL;
		dialog_vars.no_label = NULL;

		return (!retval); /* Editor -> return 0 */
	}

	swapsize = SWAP_SIZE(available);
	humanize_number(swapsizestr, 7, swapsize, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	humanize_number(rootsizestr, 7, available - swapsize - 1024*1024,
	    "B", HN_AUTOSCALE, HN_NOSPACE | HN_DECIMAL);

	geom_gettree(&submesh);
	pp = provider_for_name(&submesh, disk);
	gpart_create(pp, "freebsd-ufs", rootsizestr, "/", NULL, 0);
	geom_deletetree(&submesh);

	geom_gettree(&submesh);
	pp = provider_for_name(&submesh, disk);
	gpart_create(pp, "freebsd-swap", swapsizestr, NULL, NULL, 0);
	geom_deletetree(&submesh);

	return (0);
}

