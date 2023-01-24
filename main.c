#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

WINDOW *shared_win;
WINDOW *shared_win2;
WINDOW *blk_preview;
int main_menu_height = 16, main_menu_width = 30; //button structs would be nice too
int diff_menu_height = 16, diff_menu_width = 30;
int main_but_size[4] = {5, 17, 4};
int diff_but_size[3] = {2, 2};
int diff_ent_size[3] = {14, 19};
int diff_ent_offset[3] = {12, 18};
int status_menu_startx;
int status_menu_width;
int state = 0;
int dim = 11;
int preview_dim = 5;
int score = 0;
int hscore = 0;
int cblkno;
int curx, cury;
int cur_but;
int **frame;
int **preview_frame;
int timelimit = 1;
pthread_barrier_t timer_barrier;

struct block {

    int *px[3];
    int pxno;
};

struct block *rubble;

struct block *current; //no need exists for this yet

struct block prot[25];

void initgr(int bkgd_attr) {

    initscr();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_CYAN);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    wbkgd(stdscr, COLOR_PAIR(1));

    refresh();
    return;
}

void term() {

    endwin();
    printf("Have a nice day!\n");
    exit(1);
}

WINDOW *create_box(int height, int width, int starty, int startx) {

    WINDOW *local_box;

    local_box = newwin(height, width, starty, startx);
    box(local_box, 0, 0);

    wrefresh(local_box);
    return local_box;
}

WINDOW *create_status_menu() {

    WINDOW *local_status_menu;

    int startx, starty, maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    //status_menu_width = 2*(maxx - ((maxx + (2*dim + 1)) / 2))/3;
    status_menu_width = 15;
    starty = 0;
    startx = maxx - status_menu_width;
    status_menu_startx = startx;

    local_status_menu = create_box(maxy, status_menu_width, starty, startx);
    wbkgd(local_status_menu, COLOR_PAIR(2));
    mvwprintw(local_status_menu, 1, 1, "Score: %d", score);
    mvwprintw(local_status_menu, 3, 1, "H.S.: %d", hscore);
    mvwprintw(local_status_menu, maxy - 3, 2, "Clear a row");
    if(50 <= score) wattron(local_status_menu, COLOR_PAIR(1));
    mvwprintw(local_status_menu, maxy - 2, 6, "(E)");
    if(50 <= score) wattroff(local_status_menu, COLOR_PAIR(1));

    wrefresh(local_status_menu);

    return local_status_menu;
}

WINDOW *create_main_menu(int height, int width) {

    WINDOW *main_menu;
    int startx, starty, maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    starty = (maxy - height) / 2;
    startx = (maxx - width) / 2;

    main_menu = create_box(height, width, starty, startx);
    keypad(main_menu, TRUE);

    wattron(main_menu, COLOR_PAIR(1));
    mvwprintw(main_menu, (height - 3)/2, (width - 5)/2, "Start");
    mvwprintw(main_menu, (height - 3)/2 + 2, (width - 17)/2, "Select Difficulty");
    mvwprintw(main_menu, (height - 3)/2 + 4, (width - 4)/2, "Exit");
    wattroff(main_menu, COLOR_PAIR(1));
    wmove(main_menu, (height - 3)/2, (width - 5)/2);

    wrefresh(main_menu);
    return main_menu;
}

WINDOW *create_diff_menu(int height, int width) {

    WINDOW *diff_menu;
    int startx, starty, maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    starty = (maxy - height) / 2;
    startx = (maxx - width) / 2;

    diff_menu = create_box(height, width, starty, startx);
    keypad(diff_menu, TRUE);

    mvwprintw(diff_menu, (height - 3)/2, (width - 14)/2, "Time limit: ");
    mvwprintw(diff_menu, (height - 3)/2 + 2, (width - 19)/2, "Dimensions (n*n): ");
    wattron(diff_menu, COLOR_PAIR(1));
    if(timelimit == 1) mvwprintw(diff_menu, (height - 3)/2, (width - 14)/2 + 12, "on");
    else mvwprintw(diff_menu, (height - 3)/2, (width - 14)/2 + 12, "off");
    mvwprintw(diff_menu, (height - 3)/2 + 2, (width - 19)/2 + 18, "%d", dim);
    wattroff(diff_menu, COLOR_PAIR(1));

    wrefresh(diff_menu);
    return diff_menu;
}

WINDOW *create_main_frame(int height, int width) {

    WINDOW *main_frame;
    int startx, starty, maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    starty = (maxy - height) / 2;
    startx = (maxx - width) / 2;

    main_frame = create_box(height, width, starty, startx);
    keypad(main_frame, TRUE);

    wmove(main_frame, 0, 0);

    wrefresh(main_frame);
    return main_frame;
}


WINDOW *create_preview_frame(int height, int width) {

    WINDOW *preview_frame;
    int startx, starty, maxx, maxy;
    getmaxyx(shared_win2, maxy, maxx);
    starty = (maxy - height) / 2;
    startx = (maxx - width) / 2 + status_menu_startx;

    preview_frame = create_box(height, width, starty, startx);

    wmove(preview_frame, 0, 0);

    wrefresh(preview_frame);
    return preview_frame;
}

void update_frame() {

    for(int i = 1; i <= dim; i++) {

	int j = 1;
	for(int k = 1; k <= (2*dim) - 1; k++) {

	    if(k % 2 == 1) {

		mvwaddch(shared_win, i, k, frame[i-1][j-1]);
		j++;
	    }
	}
    }
    wrefresh(shared_win);
    return;
}

void update_preview_frame() {

    for(int i = 1; i <= preview_dim; i++) {

	int j = 1;
	for(int k = 1; k <= (2*preview_dim) - 1; k++) {

	    if(k % 2 == 1) {

		mvwaddch(blk_preview, i, k, preview_frame[i-1][j-1]);
		j++;
	    }
	}
    }
    wrefresh(blk_preview);
    return;
}

void block_init(struct block *blk, int pxno, int ini[3][pxno+1]) {

    blk->pxno = pxno;
    blk->px[0] = (int*) malloc((pxno+1)*sizeof(int));
    blk->px[1] = (int*) malloc((pxno+1)*sizeof(int));
    for(int i = 0; i < pxno; i++) {

	blk->px[0][i] = ini[0][i];
	blk->px[1][i] = ini[1][i];
    }
    return;
}

void block_deinit(struct block *blk) {

    free(blk->px[0]);
    free(blk->px[1]);
    free(blk);
    return;
}

struct block create_copy_block(struct block *blk) {

    struct block blk_local;
    blk_local.pxno = blk->pxno;
    blk_local.px[0] = (int*) malloc((blk->pxno+1)*sizeof(int));
    blk_local.px[1] = (int*) malloc((blk->pxno+1)*sizeof(int));
    for(int i = 0; i < blk->pxno; i++) {

	blk_local.px[0][i] = blk->px[0][i];
	blk_local.px[1][i] = blk->px[1][i];
    }
    return blk_local;
}

void init_status_menu() {

    blk_preview = create_preview_frame(preview_dim+2, (2*preview_dim)+1);

    preview_frame = (int**) malloc(6*sizeof(int*));
    for(int i = 0; i < 5; i++) preview_frame[i] = malloc(6*sizeof(int));

    for(int i = 0; i < preview_dim; i++) {

	for(int j = 0; j < preview_dim; j++) preview_frame[i][j] = '-';
    }

    update_preview_frame();

    return;
}

void prot_init() {

    int blk1_ini[3][2] = {{0}, {0}};
    int blk2_ini[3][3] = {{0, 0}, {0, 1}};
    int blk3_ini[3][3] = {{0, 1}, {0, 0}};
    int blk4_ini[3][5] = {{0, 0, 1, 1}, {0, 1, 0, 1}};
    int blk5_ini[3][4] = {{0, 1, 1}, {1, 0, 1}};
    int blk6_ini[3][4] = {{0, 1, 1}, {0, 0, 1}};
    int blk7_ini[3][4] = {{0, 0, 1}, {0, 1, 0}};
    int blk8_ini[3][4] = {{0, 0, 1}, {0, 1, 1}};
    int blk9_ini[3][4] = {{0, 0, 0}, {0, 1, 2}};
    int blk10_ini[3][4] = {{0, 1, 2}, {0, 0, 0}};
    int blk11_ini[3][5] = {{0, 0, 0, 0}, {0, 1, 2, 3}};
    int blk12_ini[3][5] = {{0, 1, 2, 3}, {0, 0, 0, 0}};
    int blk13_ini[3][5] = {{0, 0, 1, 1}, {0, 1, 1, 2}};
    int blk14_ini[3][5] = {{0, 0, 1, 1}, {1, 2, 1, 0}};
    int blk15_ini[3][5] = {{0, 1, 1, 2}, {0, 0, 1, 1}};
    int blk16_ini[3][5] = {{1, 2, 0, 1}, {0, 0, 1, 1}};
    int blk17_ini[3][5] = {{0, 0, 0, 1}, {0, 1, 2, 1}};
    int blk18_ini[3][5] = {{0, 1, 1, 1}, {1, 0, 1, 2}};
    int blk19_ini[3][5] = {{1, 0, 1, 2}, {0, 1, 1, 1}};
    int blk20_ini[3][5] = {{0, 1, 2, 1}, {0, 0, 0, 1}};
    int blk21_ini[3][5] = {{0, 0, 0, 1}, {0, 1, 2, 0}};
    int blk22_ini[3][5] = {{0, 1, 1, 1}, {0, 0, 1, 2}};
    int blk23_ini[3][5] = {{0, 0, 0, 1}, {0, 1, 2, 2}};
    int blk24_ini[3][5] = {{1, 1, 1, 0}, {0, 1, 2, 2}};

    block_init(&prot[0], 1, blk1_ini);
    block_init(&prot[1], 2, blk2_ini);
    block_init(&prot[2], 2, blk3_ini);
    block_init(&prot[3], 4, blk4_ini);
    block_init(&prot[4], 3, blk5_ini);
    block_init(&prot[5], 3, blk6_ini);
    block_init(&prot[6], 3, blk7_ini);
    block_init(&prot[7], 3, blk8_ini);
    block_init(&prot[8], 3, blk9_ini);
    block_init(&prot[9], 3, blk10_ini);
    block_init(&prot[10], 4, blk11_ini);
    block_init(&prot[11], 4, blk12_ini);
    block_init(&prot[12], 4, blk13_ini);
    block_init(&prot[13], 4, blk14_ini);
    block_init(&prot[14], 4, blk15_ini);
    block_init(&prot[15], 4, blk16_ini);
    block_init(&prot[16], 4, blk17_ini);
    block_init(&prot[17], 4, blk18_ini);
    block_init(&prot[18], 4, blk19_ini);
    block_init(&prot[19], 4, blk20_ini);
    block_init(&prot[20], 4, blk21_ini);
    block_init(&prot[21], 4, blk22_ini);
    block_init(&prot[22], 4, blk23_ini);
    block_init(&prot[23], 4, blk24_ini);
    return;
}

struct block choose_block(int blkno) {

    struct block blk = create_copy_block(&prot[blkno]);

    int n = dim / 2;
    switch(blkno) {

	case 8:
	case 10:
	case 12:
	case 13:
	case 16:
	case 17:
	case 20:
	case 21:
	case 22:
	case 23:

	    n--;
	    break;
    }

    for(int i = 0; i < blk.pxno; i++) {

	blk.px[1][i] += n;
    }

    return blk;
}

void frame_clear() {

    for(int i = 0; i < dim; i++) {

	for(int j = 0; j < dim; j++) frame[i][j] = '-';
    }
    return;
}

void frame_paint(struct block *blk) {

    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '#';
    }
    return;
}

int frame_put_block(struct block *blk) {

    for(int i = 0; i < blk->pxno; i++) {

	for(int j = 0; j < rubble->pxno; j++) {

	    if(blk->px[0][i] == rubble->px[0][j] && blk->px[1][i] == rubble->px[1][j]) {

		return -1;
	    }
	}
    }
    frame_paint(blk);
    return 0;
}

int frame_move_block_down(struct block *blk) {

    struct block buf;
    buf = create_copy_block(blk);
    for(int i = 0; i < buf.pxno; i++) {

	buf.px[0][i]++;
    }
    for(int i = 0; i < buf.pxno; i++) {

	for(int j = 0; j < rubble->pxno; j++) {

	    if(buf.px[0][i] == rubble->px[0][j] && buf.px[1][i] == rubble->px[1][j]) {

		rubble->px[0] = (int*) realloc(rubble->px[0], (rubble->pxno+blk->pxno+1)*sizeof(int));
		rubble->px[1] = (int*) realloc(rubble->px[1], (rubble->pxno+blk->pxno+1)*sizeof(int));
		for(int k = 0; k < blk->pxno; k++) {

		    rubble->px[0][rubble->pxno+k] = blk->px[0][k];
		    rubble->px[1][rubble->pxno+k] = blk->px[1][k];
		}
		rubble->pxno += blk->pxno;
		return -1;
	    }
	}
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '-';
	blk->px[0][i]++;
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '#';
    }
    return 0;
}

void frame_move_block_right(struct block *blk) {

    struct block buf;
    buf = create_copy_block(blk);
    for(int i = 0; i < buf.pxno; i++) {

	buf.px[1][i]++;
    }
    for(int i = 0; i < buf.pxno; i++) {

	for(int j = 0; j < rubble->pxno; j++) {

	    if(buf.px[0][i] == rubble->px[0][j] && buf.px[1][i] == rubble->px[1][j]) {

		return;
	    }
	}
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '-';
	blk->px[1][i]++;
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '#';
    }
    return;
}

void frame_move_block_left(struct block *blk) {

    struct block buf;
    buf = create_copy_block(blk);
    for(int i = 0; i < buf.pxno; i++) {

	buf.px[1][i]--;
    }
    for(int i = 0; i < buf.pxno; i++) {

	for(int j = 0; j < rubble->pxno; j++) {

	    if(buf.px[0][i] == rubble->px[0][j] && buf.px[1][i] == rubble->px[1][j]) {

		return;
	    }
	}
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '-';
	blk->px[1][i]--;
    }
    for(int i = 0; i < blk->pxno; i++) {

	frame[blk->px[0][i]][blk->px[1][i]] = '#';
    }
    return;
}

void update_status_menu(int blkno) {

    if(score > hscore) hscore = score;

    for(int i = 0; i < preview_dim; i++) {

	for(int j = 0; j < preview_dim; j++) preview_frame[i][j] = '-';
    }

    struct block pblk = create_copy_block(&prot[blkno]);

    int n = preview_dim / 2;
    switch(blkno) {

	case 8:
	case 10:
	case 12:
	case 13:
	case 16:
	case 17:
	case 20:
	case 21:
	case 22:
	case 23:

	    n--;
	    break;
    }

    for(int i = 0; i < pblk.pxno; i++) {

	pblk.px[1][i] += n;
    }

    for(int i = 0; i < pblk.pxno; i++) {

	preview_frame[pblk.px[0][i]][pblk.px[1][i]] = '#';
    }

    for(int i = 0; i < status_menu_width - 9; i++) mvwprintw(shared_win2, 1, 8+i, " ");
    for(int i = 0; i < status_menu_width - 8; i++) mvwprintw(shared_win2, 3, 7+i, " ");
    mvwprintw(shared_win2, 1, 8, "%d", score);
    mvwprintw(shared_win2, 3, 7, "%d", hscore);
    int maxx, maxy;
    getmaxyx(shared_win2, maxy, maxx);
    if(50 <= score) wattron(shared_win2, COLOR_PAIR(1));
    mvwprintw(shared_win2, maxy - 2, 6, "(E)");
    if(50 <= score) wattroff(shared_win2, COLOR_PAIR(1));

    update_preview_frame();
    wrefresh(shared_win2);

    return;
}

void resize() {

    //delwin(shared_win);
    //delwin(shared_win2);
    //delwin(blk_preview);
    erase();
    endwin(); //we only need use endwin here. erase does the trick when no resize is happening

    initgr(0);

    if(state == 0) {

	shared_win = create_main_menu(main_menu_height, main_menu_width);
	mvwchgat(shared_win, (main_menu_height - 3)/2 + (2*cur_but) - 2, (main_menu_width - main_but_size[cur_but-1])/2, main_but_size[cur_but-1], A_BOLD, 2, NULL); //rudimentary window restoration
	wmove(shared_win, cury, curx);

	wrefresh(shared_win);
	flushinp();
    }
    else if(state == 1) {

	shared_win = create_main_frame(dim+2, (2*dim)+1);
	shared_win2 = create_status_menu();
	init_status_menu();
	update_status_menu(cblkno);

	update_frame();
	flushinp();
    }
    else if(state == 2) { //it will bug if changes to dimensions are not submitted

	shared_win = create_diff_menu(diff_menu_height, diff_menu_width);
	mvwchgat(shared_win, (diff_menu_height - 3)/2 + (2*cur_but) - 2, (diff_menu_width - diff_ent_size[cur_but-1])/2 + diff_ent_offset[cur_but-1], diff_but_size[cur_but-1], A_BOLD, 2, NULL);
	wmove(shared_win, cury, curx);

	wrefresh(shared_win);
	flushinp();
    }

    return;
}

void select_difficulty() {

    erase();
    initgr(0);
    state = 2;

    int cur_but_saved = cur_but;
    int curx_saved = curx;
    int cury_saved = cury;

    shared_win = create_diff_menu(diff_menu_height, diff_menu_width);

    cur_but = 1;
    int mov_but = 1;
    int button_pressed = 0;
    mvwchgat(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, diff_but_size[0], A_BOLD, 2, NULL);

    while(1) {

	int key = wgetch(shared_win);
	if(key == KEY_MOUSE) {

	    MEVENT event;
	    getmouse(&event);
	    if(wenclose(shared_win, event.y, event.x)) {

		wmouse_trafo(shared_win, &event.y, &event.x, FALSE);
		if(event.y == (diff_menu_height - 3)/2 && ((diff_menu_width - 14)/2 + 12 <= event.x && event.x < (diff_menu_width - 14)/2 + 12 + diff_but_size[0])) {

		    mov_but = 1;
		    if(event.bstate & BUTTON1_DOUBLE_CLICKED) button_pressed = 1;
		}
		else if(event.y == (diff_menu_height - 3)/2 + 2 && ((diff_menu_width - 19)/2 + 18 <= event.x && event.x < (diff_menu_width - 19)/2 + 18 + diff_but_size[1])) {

		    mov_but = 2;
		    if(event.bstate & BUTTON1_DOUBLE_CLICKED) button_pressed = 1;
		}
		else continue;
	    }
	    else continue;
	}
	else if(key == KEY_DOWN && cur_but != 2) {

	    mov_but++;
	}
	else if(key == KEY_UP && cur_but != 1) {

	    mov_but--;
	}
	else if(key == 27) {

	    cur_but = cur_but_saved;
	    state = 0;
	    curx = curx_saved;
	    cury = cury_saved;
	    resize();
	    return;
	}
	if(cur_but != mov_but) {

	    mvwchgat(shared_win, (diff_menu_height - 3)/2 + (2*cur_but) - 2, (diff_menu_width - diff_ent_size[cur_but-1])/2 + diff_ent_offset[cur_but-1], diff_but_size[cur_but-1], A_NORMAL, 1, NULL);
	    mvwchgat(shared_win, (diff_menu_height - 3)/2 + (2*mov_but) - 2, (diff_menu_width - diff_ent_size[mov_but-1])/2 + diff_ent_offset[mov_but-1], diff_but_size[mov_but-1], A_BOLD, 2, NULL);
	    wrefresh(shared_win);

	    cur_but = mov_but;
	    cury = (diff_menu_height - 3)/2 + (2*cur_but) - 2;
	    curx = (diff_menu_width - diff_ent_size[cur_but-1])/2 + diff_ent_offset[cur_but-1];
	}
	if(key == '\n' || button_pressed) {

	    if(cur_but == 1) {

		diff_but_size[0] = 2 + timelimit;
		timelimit = !timelimit;
		if(timelimit == 1) {

		    mvwprintw(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, "   ");
		    mvwprintw(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, "on");
		    mvwchgat(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, 2, A_BOLD, 2, NULL);
		}
		else {

		    mvwprintw(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, "  ");
		    mvwprintw(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, "off");
		    mvwchgat(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12, 3, A_BOLD, 2, NULL);
		}
		wmove(shared_win, (diff_menu_height - 3)/2, (diff_menu_width - 14)/2 + 12);
		wrefresh(shared_win);
	    }
	    else if(cur_but == 2) {

		int dim_buf = dim;
		while(1) {

		    key = wgetch(shared_win);
		    wattron(shared_win, COLOR_PAIR(2) | A_BOLD);
		    if(key == KEY_DOWN) {

			if(dim_buf != 11) {

			    dim_buf--;
			    mvwprintw(shared_win, (diff_menu_height - 3)/2 + 2, (diff_menu_width - 19)/2 + 18, "%d", dim_buf);
			}
		    }
		    else if(key == KEY_UP) {

			if(dim_buf != 99) {

			    dim_buf++;
			    mvwprintw(shared_win, (diff_menu_height - 3)/2 + 2, (diff_menu_width - 19)/2 + 18, "%d", dim_buf);
			}
		    }
		    else if(key == '\n') {

			wattroff(shared_win, COLOR_PAIR(2) | A_BOLD);
			dim = dim_buf;
			break;
		    }
		    else if(key == 27) {

			mvwprintw(shared_win, (diff_menu_height - 3)/2 + 2, (diff_menu_width - 19)/2 + 18, "%d", dim);
			wattroff(shared_win, COLOR_PAIR(2) | A_BOLD);
			wmove(shared_win, (diff_menu_height - 3)/2 + 2, (diff_menu_width - 19)/2 + 18);
			wrefresh(shared_win);
			break;
		    }
		    wmove(shared_win, (diff_menu_height - 3)/2 + 2, (diff_menu_width - 19)/2 + 18);
		    wrefresh(shared_win);
		}
	    }
	}
    }
}

void *timer() {

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    while(1) {

	usleep(100000);
	pthread_barrier_wait(&timer_barrier);
    }
}

void start_game() {

    erase();
    initgr(0);
    if(timelimit == 1) halfdelay(1);
    state = 1;

    frame = (int**) malloc((dim+1)*sizeof(int*));
    for(int i = 0; i < dim; i++) frame[i] = malloc((dim+1)*sizeof(int));
    shared_win = create_main_frame(dim+2, (2*dim)+1);

    shared_win2 = create_status_menu();
    init_status_menu();

    for(int i = 0; i < dim; i++) {

	for(int j = 0; j < dim; j++) frame[i][j] = '-';
    }

    rubble = (struct block*) malloc(sizeof(struct block));
    rubble->pxno = 3*dim;
    rubble->px[0] = (int*) malloc((3*dim + 1)*sizeof(int));
    rubble->px[1] = (int*) malloc((3*dim + 1)*sizeof(int));
    for(int i = 0; i < dim; i++) rubble->px[0][i] = i;
    for(int i = 0; i < dim; i++) rubble->px[0][dim+i] = dim;
    for(int i = 0; i < dim; i++) rubble->px[0][(2*dim)+i] = i;
    for(int i = 0; i < dim; i++) rubble->px[1][i] = -1;
    for(int i = 0; i < dim; i++) rubble->px[1][dim+i] = i;
    for(int i = 0; i < dim; i++) rubble->px[1][(2*dim)+i] = dim;

    pthread_barrier_init(&timer_barrier, NULL, timelimit+1);
    pthread_t ptid;
    if(timelimit == 1) pthread_create(&ptid, NULL, &timer, NULL);

    int chpx = rubble->pxno;
    int row[dim+1];
    for(int i = 0; i < dim; i++) row[i] = 0;

    srand(time(NULL));
    cblkno = rand() % 24;

    int gameover = 0;
    while(1) {

	for(int i = chpx; i < rubble->pxno; i++) {

	    row[rubble->px[0][i]]++;
	}
	chpx = rubble->pxno;
	for(int i = 0; i < dim; i++) {

	    if(row[i] == dim) {

		score += 10;
		struct block *rubble_new;
		rubble_new = (struct block*) malloc(sizeof(struct block));
		rubble_new->pxno = rubble->pxno - dim;
		rubble_new->px[0] = (int*) malloc((rubble->pxno + 1 - dim)*sizeof(int));
		rubble_new->px[1] = (int*) malloc((rubble->pxno + 1 - dim)*sizeof(int));
		for(int j = 0; j < 3*dim; j++) {

		    rubble_new->px[0][j] = rubble->px[0][j];
		    rubble_new->px[1][j] = rubble->px[1][j];
		}
		int k = 3*dim;
		frame_clear();
		for(int j = 3*dim; j < rubble->pxno; j++) {

		    if(rubble->px[0][j] != i) {

			if(rubble->px[0][j] < i) {

			    rubble_new->px[0][k] = rubble->px[0][j] + 1;
			}
			else {

			    rubble_new->px[0][k] = rubble->px[0][j];
			}
			rubble_new->px[1][k] = rubble->px[1][j];
			frame[rubble_new->px[0][k]][rubble_new->px[1][k]] = '#';
			k++;
		    }
		}
		block_deinit(rubble);
		rubble = rubble_new;
		chpx = 3*dim;
		for(int j = 0; j < dim; j++) row[j] = 0;
		update_frame();
		break;
	    }
	}

	struct block blk = choose_block(cblkno);
	cblkno = rand() % 24;
	update_status_menu(cblkno);

	if(frame_put_block(&blk) == -1) {

	    gameover = 1;
	    break;
	}
	update_frame();
	int done = 0;
	while(1) {

	    for(int i = 0; i < 10 || timelimit == 0; i++) {

		int key = wgetch(shared_win); //ESC key problem
		if(key == KEY_RIGHT || key == 'd') frame_move_block_right(&blk);
		else if(key == KEY_LEFT || key == 'a') frame_move_block_left(&blk);
		else if(key == KEY_DOWN || key == 's') {

		    while(1) {

			if(frame_move_block_down(&blk) == -1) {

			    done = 1;
			    break;
			}
			update_frame();
			key = wgetch(shared_win);
			if(!(key == 's' || key == KEY_DOWN)) break;
		    }
		    if(done == 1) break;
		    if(key == KEY_RIGHT || key == 'd') frame_move_block_right(&blk);
		    else if(key == KEY_LEFT || key == 'a') frame_move_block_left(&blk);
		}
		else if(key == 'e' && 50 <= score) {

		    score -= 50;
		    struct block *rubble_new;
		    rubble_new = (struct block*) malloc(sizeof(struct block));
		    int n = 0;
		    for(int j = 3*dim; j < rubble->pxno; j++) if(rubble->px[0][j] == dim - 1) n++;
		    rubble_new->pxno = rubble->pxno - n;
		    rubble_new->px[0] = (int*) malloc((rubble->pxno + 1 - n)*sizeof(int));
		    rubble_new->px[1] = (int*) malloc((rubble->pxno + 1 - n)*sizeof(int));
		    for(int j = 0; j < 3*dim; j++) {

			rubble_new->px[0][j] = rubble->px[0][j];
			rubble_new->px[1][j] = rubble->px[1][j];
		    }
		    int k = 3*dim;
		    frame_clear();
		    for(int j = 3*dim; j < rubble->pxno; j++) {

			if(rubble->px[0][j] != dim - 1) {

			    rubble_new->px[0][k] = rubble->px[0][j] + 1;
			    rubble_new->px[1][k] = rubble->px[1][j];
			    frame[rubble_new->px[0][k]][rubble_new->px[1][k]] = '#';
			    k++;
			}
		    }
		    block_deinit(rubble);
		    rubble = rubble_new;
		    chpx = 3*dim;
		    for(int j = 0; j < dim; j++) row[j] = 0;
		    frame_paint(&blk);
		    update_status_menu(cblkno);
		    update_frame();
		}
		update_frame();
		pthread_barrier_wait(&timer_barrier);
	    }
	    if(done == 1) break;
	    if(frame_move_block_down(&blk) == -1) break;
	}
    }
    if(gameover == 1) {

	WINDOW *msg_win;
	int startx, starty, maxx, maxy;
	getmaxyx(stdscr, maxy, maxx);
	starty = (maxy - 3) / 2;
	startx = (maxx - (2*dim+1)) / 2;

	msg_win = create_box(3, 2*dim+1, starty, startx);
	wbkgd(msg_win, COLOR_PAIR(3));
	mvwprintw(msg_win, 1, (2*dim+1)/2 - 4, "Game Over!");
	wrefresh(msg_win);
    }

    if(timelimit == 1) pthread_cancel(ptid);

    free(frame);
    block_deinit(rubble);

    cbreak(); //black magic! disables halfdelay mode
    wgetch(shared_win);
    state = 0;
    score = 0;
    resize();
    return;
}

void main_menu() {

    shared_win = create_main_menu(main_menu_height, main_menu_width);

    cury = (main_menu_height - 3)/2;
    curx = (main_menu_width - 5)/2;
    cur_but = 1;
    int mov_but = 1;
    int button_pressed = 0;
    mvwchgat(shared_win, (main_menu_height - 3)/2 + (2*cur_but) - 2, (main_menu_width - 5)/2, 5, A_BOLD, 2, NULL);

    while(1) {

	int key = wgetch(shared_win);
	if(key == KEY_MOUSE) {

	    MEVENT event;
	    getmouse(&event);
	    if(wenclose(shared_win, event.y, event.x)) {

		wmouse_trafo(shared_win, &event.y, &event.x, FALSE);
		if(event.y == (main_menu_height - 3)/2 && ((main_menu_width - 5)/2 <= event.x && event.x < (main_menu_width - 5)/2 + 5)) {

		    mov_but = 1;
		    if(event.bstate & BUTTON1_DOUBLE_CLICKED) button_pressed = 1;
		}
		else if(event.y == (main_menu_height - 3)/2 + 2 && ((main_menu_width - 17)/2 <= event.x && event.x < (main_menu_width - 17)/2 + 17)) {

		    mov_but = 2;
		    if(event.bstate & BUTTON1_DOUBLE_CLICKED) button_pressed = 1;
		}
		else if(event.y == (main_menu_height - 3)/2 + 4 && ((main_menu_width - 4)/2 <= event.x && event.x < (main_menu_width - 4)/2 + 4)) {

		    mov_but = 3;
		    if(event.bstate & BUTTON1_DOUBLE_CLICKED) button_pressed = 1;
		}
		else continue;
	    }
	    else continue;
	}
	else if(key == KEY_DOWN && cur_but != 3) {

	    mov_but++;
	}
	else if(key == KEY_UP && cur_but != 1) {

	    mov_but--;
	}
	if(cur_but != mov_but) {

	    mvwchgat(shared_win, (main_menu_height - 3)/2 + (2*cur_but) - 2, (main_menu_width - main_but_size[cur_but-1])/2, main_but_size[cur_but-1], A_NORMAL, 1, NULL);
	    mvwchgat(shared_win, (main_menu_height - 3)/2 + (2*mov_but) - 2, (main_menu_width - main_but_size[mov_but-1])/2, main_but_size[mov_but-1], A_BOLD, 2, NULL);
	    wrefresh(shared_win);

	    cur_but = mov_but;
	    cury = (main_menu_height - 3)/2 + (2*cur_but) - 2;
	    curx = (main_menu_width - main_but_size[cur_but-1])/2;
	}
	if(key == '\n' || button_pressed) {

	    if(cur_but == 3) term();
	    if(cur_but == 1) {

		state = 1;
		start_game();
	    }
	    if(cur_but == 2) select_difficulty();
	    button_pressed = 0;
	}
    }
}

int main() {

    initgr(0);
    cbreak();
    noecho();
    mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED, NULL);

    signal(SIGWINCH, resize);
    signal(SIGINT, term);

    prot_init();

    main_menu();
}