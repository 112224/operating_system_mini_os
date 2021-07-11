#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define TOTALFORK 1

typedef struct lcrs* ptr_lcrs;
typedef struct lcrs				//파일, dir 형식의 구조체 lcrs
{
	int chmod;
	char *name;			//이름 사이즈 동적할당으로 변경
	char *contents;
	bool hide;			//숨김여부
	bool type;			//true = file, false = dir
	ptr_lcrs child, sib;
	struct tm* t;
}lcrs;

ptr_lcrs root, cur;

char st[1024] = "/";	//기본경로 /
int st_num = 0;	 st_max = 0;	//st_num 은 항상 스택의 끝자리 (\0 제외)
void setEmpty()
{
	st_num = -1;
	memset(st, 0, sizeof(st));	//cd 명령어 구현시 사용.
}
bool push(const char *com_path)
{
	int len = strlen(com_path);   //strlen 은 \0 전까지만 셈.
	if (len + st_num >= 1024)
	{
		printf("name or path too long!\ntotal max length is 1024\n");
		return false;
	}
	else
	{
		strcat(st, com_path);
		//if (st_num < 0)st_num = 0;
		st_num += len;
		if (st_num > st_max)st_max = st_num;
		return true;
	}
}
void pop(int len)
{
	while (len--)
		st[st_num--] = 0;
}

ptr_lcrs make(const char *name, bool type, bool opt, int val)      //type에 따라 dir이나 file 생성
{
	time_t timer;
	timer = time(NULL); // date
	struct tm *temptime = (struct tm*)malloc(sizeof(struct tm));
	ptr_lcrs ret = (ptr_lcrs)malloc(sizeof(lcrs));
	if (ret != NULL)
	{
		memcpy(temptime, localtime(&timer), sizeof(struct tm));
		ret->t = temptime;
		ret->name = (char*)malloc(sizeof(char)*(strlen(name) + 3));   //lcrs_name 부분 수정관련
		strcpy(ret->name, name);
		if (opt)ret->chmod = val;
		else ret->chmod = 755;
		ret->type = type;
		ret->child = ret->sib = NULL;
		ret->contents = NULL;         ////
		if (name[0] == '.')
			ret->hide = true;
		else
			ret->hide = false;
		return ret;
	}
	return NULL;
}


void pwd(int i)		//pwd 완료. 스택에는 cur의 경로가 들어감.
{
	if (i == 1) puts(st);
	else printf("%s:", st);

}

ptr_lcrs makeRoot();
ptr_lcrs _makeDir(ptr_lcrs now, char *name, char *opt, int val, bool last);
bool cd(char *path);
bool relative(ptr_lcrs now, const char *name, int op, bool type);
ptr_lcrs setPath();
bool ls(char *opt, char *path);
bool cat(const char* option, const char* filename);
bool rm(char *opt, char *name);
void rm_component(ptr_lcrs now);
bool mv(const char *mvpara1, const char *mvpara2);
bool cp(const char *cpypara1, const char *cpypara2);
void dividefnamedir(char* string, char* filename);
void find(char *path, char *opt, char *name);
void preorder(ptr_lcrs now, ptr_lcrs pare, bool op, bool fL, bool fR, char *name, bool done);
bool cmd(char *comm);
bool chmod(int val, char* name);
bool touch(char* name);
void modtostr(int chmod);
void save(ptr_lcrs now, ptr_lcrs pare);
void save_module(ptr_lcrs now);
void save_module(ptr_lcrs now);
ptr_lcrs load_module(ptr_lcrs now, ptr_lcrs pare);


////스레드 관련 함수, 전역 변수들
void *mkdirthrfunc(void *arg);
void *rmthrfunc(void *arg);
void *catthrfunc(void *arg);
void *chmodthrfunc(void *arg);
void *touchthrfunc(void *arg);
pthread_mutex_t lock;
pthread_mutexattr_t mutex_attr;
int cnt;
int num;
int linecnt;
char c[6][50];	////cmd의 c부분 스레드 함수에서 편하게 쓰려고 전역변수로 만들었어요


int main()
{
	st[0] = '%';
	cur = makeRoot(); root = cur;
	root = load_module(root, root);
	if (root != NULL)
	{
		cur = root;
	}
	else
		root = cur;
	setEmpty(); push("/");
	while (1)
	{
		char comm[50];
		pwd(0);
		gets(comm);
		if (!cmd(comm))break;
	}
	rm_component(root);
}
void modtostr(int chmod)
{
	int chm = chmod;
	int k = 100;
	int i = 0;
	int num;
	while (i < 3)
	{
		num = chm / k;
		chm = chm - num * k;
		k = k / 10;
		switch (num)
		{
		case 0:
			printf("---");
			break;
		case 1:
			printf("--x");
			break;
		case 2:
			printf("-w-");
			break;
		case 3:
			printf("-wx");
			break;
		case 4:
			printf("r--");
			break;
		case 5:
			printf("r-x");
			break;
		case 6:
			printf("rw-");
			break;
		case 7:
			printf("rwx");
			break;
		}
		i++;
	}
	printf("  ");
}


ptr_lcrs makeRoot()		//root의 경우엔 현재노드같은게 없기에 바로 생성해줌
{
	ptr_lcrs tmp;
	tmp = make("", false, false, 0);
	if (tmp == NULL) {
		printf("!!!error !!!\n");
		return NULL;
	}
	tmp->child = make(".", false, false, 0); tmp->child->sib = make("..", false, false, 0);
	tmp->child->child = tmp; tmp->child->sib->child = tmp;  //.과 .. 처리

	return tmp;
}
ptr_lcrs _makeDir(ptr_lcrs now, char *name, char *opt, int val, bool last)	// 무조건 상대경로.	잘생성되었나 검사하기 위함
{
	char *tmp = strstr(name, "/");
	bool flagM = false, flagP = false;
	if (strstr(opt, "m") != 0) flagM = true;
	if (strstr(opt, "p") != 0)flagP = true;
	if (tmp == NULL)
	{
		if (relative(now, name, 1, false) && last) {
			puts("!!!already been!!!");
			return NULL;
		}

		ptr_lcrs ctmp = cur, tmp;
		if (!last)
		{
			if (relative(now, name, 0, false)) {
				tmp = cur;
				cur = ctmp;
				return tmp;
			}
			else {
				if (!flagP) {
					puts("!!!dir no exist!!!");
					return NULL;
				}
			}
		}
		tmp = make(name, false, flagM, val);
		tmp->child = make(".", false, false, 0); tmp->child->sib = make("..", false, false, 0);
		tmp->child->child = tmp; tmp->child->sib->child = now;  //.과 .. 처리

		ptr_lcrs pp = now->child->sib;
		while (pp->sib != NULL)
			pp = pp->sib;
		pp->sib = tmp;
		return tmp;
	}
	else
	{
		last = false;
		char *token = strtok(name, "/");
		char *next = strtok(NULL, "/");
		ptr_lcrs tp = now;
		while (token != NULL && tp != NULL)
		{
			if (next == NULL)last = true;
			tp = _makeDir(tp, token, opt, val, last);
			token = next;
			next = strtok(NULL, "/");
		}
	}
}
//mkdir
bool cd(char *path)
{
	if (strcmp(path, "") == 0)strcpy(path, "/");
	if (path[0] == '/')
	{
		char *tmp; tmp = (char*)malloc(sizeof(char)*(strlen(st) + 3));
		memset(tmp, 0, sizeof(tmp));
		tmp = strcpy(tmp, st);
		setEmpty();
		if (path[1] == '/') push("/");
		else push(path);
		ptr_lcrs tl = setPath();
		if (tl == NULL)
		{
			puts("!!!invalide directory!!!");
			setEmpty();
			strcpy(st, tmp);
			free(tmp);
			return false;
		}
		cur = tl;
		free(tmp);
		return true;
	}
	else if (path[0] == '.')
	{
		if (strcmp(path, "..") == 0)
		{
			if (cur != root) {
				int val = strlen(cur->name);
				relative(cur, "..", 0, false);
				cur = cur->child;
				if (cur == root) {
					setEmpty();
					push("/");
				}
				else
					pop(++val);
			}
		}
		return true;
	}
	else
	{
		char *token = strtok(path, "/");
		ptr_lcrs tmp = cur; int len = 0;

		while (token != NULL) {
			if (relative(cur, token, 0, false))      //여기서 cur 이 바뀌고
			{
				char *c; c = (char*)malloc(sizeof(char)*(strlen(path) + 3));
				memset(c, 0, sizeof(c));
				if (cur->child->sib->child != root)c[0] = '/';
				strcat(c, token);      //스택 수정
				push(c); len += strlen(c);
				free(c);
				token = strtok(NULL, "/");
			}
			else
			{
				puts("!!!invalide directory!!!");
				if (tmp != root && len != 0)len++;
				cur = tmp;
				pop(len);
				return false;
			}
		}
		return true;
	}
}
bool relative(ptr_lcrs now, const char *name, int op, bool type)	//type에 따라 경로 확인 op값이 0인 경우 cur 노드를 바꿔줌
{
	ptr_lcrs tmp = now->child;
	while (tmp != NULL)
	{
		if (strcmp(tmp->name, name) == 0)
		{
			if (op == 0 && type == false) cur = tmp;
			return true;
		}
		tmp = tmp->sib;
	}
	return false;
}
ptr_lcrs setPath()		//절대 경로 이용 ( / 부터의 상대 경로로 이동)
{
	ptr_lcrs ret = root;
	char *tmp; tmp = (char*)malloc(sizeof(char)*(strlen(st) + 2));
	strcpy(tmp, st);
	if (strcmp(tmp, "/") == 0) {
		free(tmp);
		return ret;
	}
	char *token = strtok(tmp, "/");
	while (token != NULL)
	{
		if (!relative(ret, token, 0, false)) {
			free(tmp);
			return NULL;
		}
		ret = cur;
		token = strtok(NULL, "/");
	}
	free(tmp);
	return ret;
}
//cd
bool ls(char *opt, char *path)		//ls 기본완료	현재 노드 기준으로 자식들을 보여줌.
{
	char tmpdir[1024]; strcpy(tmpdir, st);
	ptr_lcrs tmpcur = cur;
	if (strcmp(path, "") != 0)
	{
		bool flag = true;
		flag = cd(path);
		if (flag == false)
		{
			cd(tmpdir);
			cur = tmpcur;
			return false;
		}

	}
	bool flagL = false, flagA = false;
	if (strcmp(opt, "") != 0) {
		if (strstr(opt, "a") != 0) flagA = true;
		if (strstr(opt, "l") != 0) flagL = true;
	}
	if (cur->child->sib->sib == NULL)
	{
		printf("Directory is empty.\n");
		cd(tmpdir);
		cur = tmpcur;
		return false;
	}
	if (!flagA)				////a옵션이 안 들어왔다면
	{

		ptr_lcrs tmp = cur->child;
		if (!flagL)				////l옵션과 a옵션이 안 들어왔다면
		{
			while (tmp != NULL)
			{
				if (tmp->hide == false)
					printf("%s\t", tmp->name);
				tmp = tmp->sib;
			}
		}
		else                       ////l옵션이 들어왔다면
		{
			while (tmp != NULL)
			{
				if (tmp->hide == false)
				{
					if (tmp->type == true) printf("-");
					else printf("d");
					modtostr(tmp->chmod);
					printf("%02d-%02d %02d:%02d:%02d    ", tmp->t->tm_mon + 1, tmp->t->tm_mday, tmp->t->tm_hour, tmp->t->tm_min, tmp->t->tm_sec);
					printf("%s\n", tmp->name);
				}
				tmp = tmp->sib;
			}
		}
	}
	else
	{
		ptr_lcrs tmp = cur->child;
		if (!flagL)				////a옵션이 들어왔는데 l이 안들어왔다면
		{
			while (tmp != NULL)
			{
				printf("%s\t", tmp->name);
				tmp = tmp->sib;
			}
		}
		else                       ////l과 a모두 들어왔다면
		{
			while (tmp != NULL)
			{
				if (tmp->type == true) printf("-");
				else printf("d");
				modtostr(tmp->chmod);
				printf("%02d-%02d %02d:%02d:%02d    ", tmp->t->tm_mon + 1, tmp->t->tm_mday, tmp->t->tm_hour, tmp->t->tm_min, tmp->t->tm_sec);
				printf("%s\n", tmp->name);
				tmp = tmp->sib;
			}
		}
	}
	printf("\n");
	cd(tmpdir);
	cur = tmpcur;
	return true;
}
//ls
bool cat(const char* opt, const char* filename)
{

	if (strcmp(opt, "") == 0 && strcmp(filename, "") == 0)
	{
		char s;
		while (~scanf("%c", &s))
		{
			printf("%c", s);
			if (s == '\4x')break;
		}
		while (getchar() != '\n');
	}
	if (strcmp(opt, "") == 0 && strcmp(filename, "") != 0)
	{
		if (!relative(cur, filename, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		ptr_lcrs targetfile = cur->child->sib->sib;
		while (targetfile != NULL)
		{
			if (strcmp(targetfile->name, filename) == 0 && targetfile->type == true) break;
			targetfile = targetfile->sib;
		}
		printf("%s", targetfile->contents);

		return true;
	}
	else if ((strcmp(">", opt) == 0) && strcmp(filename, "") != 0)
	{
		ptr_lcrs tmpfile, tmplocation;
		tmpfile = make(filename, true, false, 0);

		char text[1024]; //memset(text, 0, sizeof(text));

		scanf("%[^'\x4']", text);
		while (getchar() != '\n');
		tmpfile->contents = (char*)malloc(sizeof(char)*(strlen(text) + 5));
		strcpy(tmpfile->contents, text);

		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = tmpfile;

		return true;

	}

	else if ((strcmp("-n", opt) == 0) && strcmp(filename, "") != 0)
	{
		if (!relative(cur, filename, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		ptr_lcrs targetfile;
		/*int i = 1;*/
		int j = 0;
		targetfile = cur->child->sib->sib;
		while (targetfile != NULL)
		{
			if (strcmp(targetfile->name, filename) == 0 && targetfile->type == true) break;
			targetfile = targetfile->sib;
		}
		int len = strlen(targetfile->contents);
		char *ret = targetfile->contents;
		printf("\t%d : ", linecnt++);
		while (j < len)
		{
			printf("%c", ret[j]);
			if (ret[j] == '\n')
				if (j + 1 != len)
					printf("\t%d : ", linecnt++);
			j++;
		}
		return true;
	}
	return false;
}
//cat
bool rm(char *opt, char *name)
{
	bool flagR = false, flagF = false;	//옵션 r과 f 설정
	if (strcmp(opt, "") != 0) {
		if (strstr(opt, "r") != 0) flagR = true;
		if (strstr(opt, "f") != 0) flagF = true;
	}
	ptr_lcrs target = cur->child->sib;
	ptr_lcrs prev = cur->child;
	while (target != NULL) {
		if (strcmp(target->name, name) == 0)
			break;
		prev = target;
		target = target->sib;
	}
	if (target == NULL) {
		puts("!!!no such file or dir!!!");
		return false;
	}
	if (!flagF) {
		if (!flagR)
		{
			if (target->type == true)
			{
				char buf[10];
				puts("really delete?:(y/n)"); gets(buf);
				if (buf[0] == 'y')
				{
					prev->sib = target->sib;
					if (target->contents != NULL)free(target->contents);
					free(target->t);
					free(target);
					return true;
				}
				return false;
			}
			else
			{
				puts("!!!that is dir!!!");
				return false;
			}
		}
		else
		{
			char buf[10];
			puts("really delete?:(y/n)"); gets(buf);
			if (buf[0] == 'y')
			{
				prev->sib = target->sib;
				rm_component(target); return true;
			}
			return false;
		}
	}
	else
	{
		if (!flagR) {
			if (target->type == true)
			{
				prev->sib = target->sib;
				if (target->contents != NULL)free(target->contents);
				free(target->t);
				free(target);
				return true;
			}
			else {
				puts("!!!that is dir!!!");
				return true;
			}
		}
		else {
			prev->sib = target->sib;
			rm_component(target);
		}
	}
	return true;
}

void rm_component(ptr_lcrs now)
{
	static int recursivcnt = 0;
	if (now == NULL)
	{
		recursivcnt--;
		return;
	}
	if (now->contents != NULL)
		free(now->contents);
	if ((now->name)[0] != '.')
	{
		recursivcnt++;
		rm_component(now->child);
	}
	if (recursivcnt != 0)
	{
		recursivcnt++;
		rm_component(now->sib);
	}
	free(now->t);
	free(now);
	if (recursivcnt != 0) recursivcnt--;
}
//rm
bool mv(const char *mvpara1, const char *mvpara2)
{

	if ((strstr(mvpara1, "/") == 0) && (strstr(mvpara2, "/") == 0))	////입력받은 파라미터가 둘 다 filename인 경우
	{
		if (!relative(cur, mvpara1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		else
		{
			if (strcmp(mvpara1, mvpara2) == 0)
			{
				printf("Both of name are same each other!!\n");
				return false;
			}
			ptr_lcrs orifile;
			orifile = cur->child->sib->sib;
			while (orifile != NULL) {
				if (strcmp(orifile->name, mvpara1) == 0 && orifile->type == true) break;
				orifile = orifile->sib;
			}
			strcpy(orifile->name, mvpara2);
			return true;
		}
	}
	else if ((strstr(mvpara1, "/") == 0) && (strstr(mvpara2, "/") != 0))	//// 첫번째만 filename 2번째가 dir인 경우
	{
		char tmpdir[1024];
		ptr_lcrs tmpcur = cur;
		strcpy(tmpdir, st);
		char filename[50];
		char dir[50];
		strcpy(dir, mvpara2);
		dividefnamedir(dir, filename);

		ptr_lcrs orifile, cpyfile, tmplocation, prev;
		if (!relative(cur, mvpara1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;

		}
		prev = cur->child->sib;
		orifile = cur->child->sib->sib;
		while (orifile != NULL) {
			if (strcmp(orifile->name, mvpara1) == 0 && orifile->type == true) break;
			prev = orifile;
			orifile = orifile->sib;
		}
		cpyfile = make(filename, true, false, 0);
		cpyfile->contents = (char*)malloc(sizeof(char) * ((strlen(orifile->contents) + 1)));
		strcpy(cpyfile->contents, orifile->contents);

		cd(dir);
		if (relative(cur, filename, 1, true))
		{
			printf("The file you want to duplicate is already exists !\n");
			return false;
		}
		prev->sib = orifile->sib;
		free(orifile->t);
		free(orifile->contents);
		free(orifile);

		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = cpyfile;

		cd(tmpdir);
		cur = tmpcur;
		return true;

	}
	else if ((strstr(mvpara1, "/") != 0) && (strstr(mvpara2, "/") != 0))	////둘 다 dir
	{
		char tmpdir[1024];
		ptr_lcrs tmpcur = cur;
		strcpy(tmpdir, st);
		char filename1[50], filename2[50];
		char dir1[50], dir2[50];
		strcpy(dir1, mvpara1);
		strcpy(dir2, mvpara2);

		dividefnamedir(dir1, filename1);
		dividefnamedir(dir2, filename2);

		cd(dir1);
		if (!relative(cur, filename1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		ptr_lcrs orifile, cpyfile, tmplocation, prev;
		prev = cur->child->sib;
		orifile = cur->child->sib->sib;
		while (orifile != NULL) {
			if (strcmp(orifile->name, filename1) == 0 && orifile->type == true) break;
			prev = orifile;
			orifile = orifile->sib;
		}

		cpyfile = make(filename2, true, false, 0);
		cpyfile->contents = (char*)malloc(sizeof(char) * ((strlen(orifile->contents) + 1)));
		strcpy(cpyfile->contents, orifile->contents);

		cd(dir2);
		if (relative(cur, filename2, 1, true))
		{
			printf("The file you want to duplicate is already exists !\n");
			return false;
		}

		prev->sib = orifile->sib;
		free(orifile->t);
		free(orifile->contents);
		free(orifile);

		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = cpyfile;

		cd(tmpdir);
		cur = tmpcur;
		return true;
	}
	return false;






}
bool cp(const char *cpypara1, const char *cpypara2)
{

	if ((strstr(cpypara1, "/") == 0) && (strstr(cpypara2, "/") == 0))	////입력받은 파라미터가 둘 다 filename인 경우
	{
		if (!relative(cur, cpypara1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		else
		{
			if (strcmp(cpypara1, cpypara2) == 0)
			{
				printf("Both of name are same each other!!\n");
				return false;
			}
			ptr_lcrs orifile, cpyfile, tmplocation;
			orifile = cur->child->sib->sib;
			while (orifile != NULL) {
				if (strcmp(orifile->name, cpypara1) == 0 && orifile->type == true) break;
				orifile = orifile->sib;
			}
			cpyfile = make(cpypara2, true, false, 0);
			cpyfile->contents = (char*)malloc(sizeof(char) * (/*sizeof*/(strlen(orifile->contents) + 1)));
			strcpy(cpyfile->contents, orifile->contents);

			tmplocation = cur->child->sib;
			while (tmplocation->sib != NULL)
				tmplocation = tmplocation->sib;
			tmplocation->sib = cpyfile;

			return true;
		}
	}
	else if ((strstr(cpypara1, "/") == 0) && (strstr(cpypara2, "/") != 0))	//// 첫번째만 filename 2번째가 dir인 경우
	{
		char tmpdir[1024];
		ptr_lcrs tmpcur = cur;
		strcpy(tmpdir, st);
		char filename[50];
		char dir[50];
		strcpy(dir, cpypara2);
		dividefnamedir(dir, filename);

		ptr_lcrs orifile, cpyfile, tmplocation;
		if (!relative(cur, cpypara1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;

		}
		orifile = cur->child->sib->sib;
		while (orifile != NULL) {
			if (strcmp(orifile->name, cpypara1) == 0 && orifile->type == true) break;
			orifile = orifile->sib;
		}
		cpyfile = make(filename, true, false, 0);
		cpyfile->contents = (char*)malloc(sizeof(char) * (/*sizeof*/(strlen(orifile->contents) + 1)));
		strcpy(cpyfile->contents, orifile->contents);

		cd(dir);
		if (relative(cur, filename, 1, true))
		{
			printf("The file you want to duplicate is already exists !\n");
			return false;
		}
		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = cpyfile;


		cd(tmpdir);
		cur = tmpcur;
		return true;

	}
	else if ((strstr(cpypara1, "/") != 0) && (strstr(cpypara2, "/") != 0))	////둘 다 dir
	{
		char tmpdir[1024];
		ptr_lcrs tmpcur = cur;
		strcpy(tmpdir, st);
		char filename1[50], filename2[50];
		char dir1[50], dir2[50];
		strcpy(dir1, cpypara1);
		strcpy(dir2, cpypara2);

		dividefnamedir(dir1, filename1);
		dividefnamedir(dir2, filename2);

		cd(dir1);
		if (!relative(cur, filename1, 1, true))
		{
			printf("The file you look for is not exists!!\n");
			return false;
		}
		ptr_lcrs orifile, cpyfile, tmplocation;
		orifile = cur->child->sib->sib;
		while (orifile != NULL) {
			if (strcmp(orifile->name, filename1) == 0 && orifile->type == true) break;
			orifile = orifile->sib;
		}

		cpyfile = make(filename2, true, false, 0);
		cpyfile->contents = (char*)malloc(sizeof(char) * (/*sizeof*/(strlen(orifile->contents) + 1)));
		strcpy(cpyfile->contents, orifile->contents);

		cd(dir2);
		if (relative(cur, filename2, 1, true))
		{
			printf("The file you want to duplicate is already exists !\n");
			return false;
		}
		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = cpyfile;

		cd(tmpdir);
		cur = tmpcur;
		return true;
	}
	return false;
}
void dividefnamedir(char* string, char* filename)		////경로와  file명을 구별해줌
{

	char c[6][50] = { NULL };
	int i = 0;
	char *token = strtok(string, "/");
	bool flag = true;
	while (token != NULL)
	{
		strcpy(c[i++], token);
		token = strtok(NULL, "/");
	}
	int cnt = 0;
	memset(string, 0, sizeof(string));
	while (cnt < i - 1)
	{
		strcat(string, "/");
		strcat(string, c[cnt++]);
	}
	strcpy(filename, c[i - 1]);

}
//cp
void find(char *opt, char *path, char* name)
{
	bool flag = true, flagL = false, flagR = false;
	if (strcmp(opt, "") != 0)
	{
		flag = false;			//여러 옵션을 받는다면 strstr로 처리
		if (name[0] == '*') { flagL = true; strcpy(name, name + 1); }
		if (name[strlen(name) - 1] == '*') { flagR = true; name[strlen(name) - 1] = '\0'; }
	}
	char *tmp = (char*)malloc(sizeof(char)*(strlen(st) + 3));
	strcpy(tmp, st);
	ptr_lcrs tmp_ptr = cur;
	cd(path);
	preorder(cur, cur, flag, flagL, flagR, name, false);
	cur = tmp_ptr;
	strcpy(st, tmp);

	free(tmp);
}
void preorder(ptr_lcrs now, ptr_lcrs pare, bool op, bool fL, bool fR, char *name, bool done)		//op 바로 출력 true
{
	char *tmp;
	int pos, lena, lenb;
	if (now == NULL)return;
	if ((now->name)[0] == '.')
		now = now->sib->sib;
	if (now == NULL)return;
	bool pd = done;
	if (now != pare && now->hide == false) {
		char *c; c = (char*)malloc(sizeof(char)*(strlen(now->name) + 3));
		memset(c, 0, sizeof(c));
		if (now->child->sib->child != root)c[0] = '/';
		strcat(c, now->name);		//스택 수정
		push(c);
		free(c);
	}
	if (now->hide == false) {
		if (op || done) puts(st);
		else {
			tmp = strstr(now->name, name);
			if (fL&&fR && (tmp != NULL)) { puts(st); done = true; }
			else if (fL && !fR && (tmp != NULL)) {
				pos = (int)(tmp - (now->name));
				lena = strlen(now->name); lenb = strlen(name);
				if (pos == (lena - lenb))
				{
					puts(st); done = true;
				}
			}
			else if (!fL&&fR && (tmp != NULL)) {
				pos = (int)(tmp - (now->name));
				if (pos == 0) { puts(st); done = true; }
			}
			else if (!fL && !fR && strcmp(now->name, name) == 0) { puts(st); done = true; }
		}
		preorder(now->child, pare, op, fL, fR, name, done);
	}
	if (now != pare && now->hide == false)
	{
		int lenn = strlen(now->name);
		if (now->child->sib->child != root) lenn++;
		pop(lenn);
	}
	preorder(now->sib, pare, op, fL, fR, name, pd);
}
bool chmod(int val, char* name)
{
	if ((!relative(cur, name, 1, true)) || (!relative(cur, name, 1, false)))
	{
		printf("The file or dir you look for is not exists!!\n");
		return false;
	}
	else
	{
		ptr_lcrs target, tmplocation;
		target = cur->child->sib->sib;
		while (target != NULL) {
			if (strcmp(target->name, name) == 0) break;
			target = target->sib;
		}
		target->chmod = val;
	}
}

bool touch(char* name)
{
	if ((relative(cur, name, 1, true)))
	{
		time_t timer;
		timer = time(NULL);
		struct tm* tmp;
		ptr_lcrs target;
		target = cur->child->sib->sib;
		while (target != NULL) {
			if (strcmp(target->name, name) == 0) break;
			target = target->sib;
		}
		tmp = target->t;
		struct tm *temptime = (struct tm*)malloc(sizeof(struct tm));
		memcpy(temptime, localtime(&timer), sizeof(struct tm));
		target->t = temptime;
		free(tmp);
		return true;
	}
	else
	{
		ptr_lcrs newfile, tmplocation; newfile = make(name, true, false, 0);
		tmplocation = cur->child->sib;
		while (tmplocation->sib != NULL)
			tmplocation = tmplocation->sib;
		tmplocation->sib = newfile;
		return true;
	}
	return false;
}



bool cmd(char *comm)
{
	for (int i = 0; i < 6; i++)				////thread에서 사용하려고 c를 전역변수로 바꿨어요.
		strcpy(c[i], "");					////매 턴마다 전역변수 c를 ""들로 초기화해줍니다.
	int i = 0;
	char *token = strtok(comm, " ");
	bool flag = true;
	while (token != NULL)
	{
		if (i == 1)i++;
		if (token[0] == '-' || token[0] == '>')
		{
			strcpy(c[1], token);
			token = strtok(NULL, " ");
		}
		else
		{
			strcpy(c[i++], token);
			token = strtok(NULL, " ");
		}
	}
	if (strcmp("ls", c[0]) == 0) {
		ls(c[1], c[2]);
	}
	else if (strcmp("touch", c[0]) == 0) {
		num = 2;
		cnt = 2;
		int status;
		while (strcmp(c[cnt], "") != 0) cnt++;
		pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 2));
		for (int i = 0; i < cnt - 2; i++)
		{
			if ((status = pthread_create(&tid[i], NULL, &touchthrfunc, NULL)) != 0)
			{
				printf("error occured in creating thread!!");
				exit(0);
			}
		}
		for (i = 0; i < cnt - 2; i++) pthread_join(tid[i], NULL);
		free(tid);
	}
	else if (strcmp("pwd", c[0]) == 0) {
		pwd(1);
	}
	else if (strcmp("clear", c[0]) == 0) system("clear");

	else if (strcmp("cd", c[0]) == 0) {
		cd(c[2]);
	}
	else if (strcmp("mkdir", c[0]) == 0) {
		num = 2;						////thrd함수에서 사용할 전역변수 초기화
		cnt = 2;
		int status;
		if (strstr(c[1], "m") != 0)	////옵션이 m인경우 
		{
			num = 3;
			cnt = 3;
			while (strcmp(c[cnt], "") != 0) cnt++;	////파라미터로 전달받은 폴더 갯수 카운트. 폴더는 2부터 시작함(c[2])
			pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 3));	////만드는 폴더의 갯수만큼 스레드 생성
			for (int i = 0; i < cnt - 3; i++)
			{
				if ((status = pthread_create(&tid[i], NULL, &mkdirthrfunc, NULL)) != 0)		////thrd 진입
				{
					printf("error occured in creating thread!!");
					exit(0);
				}
			}
			for (i = 0; i < cnt - 3; i++) pthread_join(tid[i], NULL);			////thrd종료 대기
			free(tid);////thread free
		}
		else
		{
			while (strcmp(c[cnt], "") != 0) cnt++;	////파라미터로 전달받은 폴더 갯수 카운트. 폴더는 2부터 시작함(c[2])
			pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 2));	////만드는 폴더의 갯수만큼 스레드 생성
			for (int i = 0; i < cnt - 2; i++)
			{
				if ((status = pthread_create(&tid[i], NULL, &mkdirthrfunc, NULL)) != 0)		////thrd 진입
				{
					printf("error occured in creating thread!!");
					exit(0);
				}
			}
			for (i = 0; i < cnt - 2; i++) pthread_join(tid[i], NULL);			////thrd종료 대기
			free(tid);////thread free
		}



	}
	else if (strcmp("rm", c[0]) == 0) {		////위 mkdir이랑 똑같은 논리입니다.
		num = 2;
		cnt = 2;
		int status;
		while (strcmp(c[cnt], "") != 0) cnt++;
		pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 2));
		for (int i = 0; i < cnt - 2; i++)
		{
			if ((status = pthread_create(&tid[i], NULL, &rmthrfunc, NULL)) != 0)
			{
				printf("error occured in creating thread!!");
				exit(0);
			}
		}
		for (i = 0; i < cnt - 2; i++) pthread_join(tid[i], NULL);
		free(tid);
	}
	else if (strcmp("mv", c[0]) == 0) {

		mv(c[2], c[3]);

	}
	else if (strcmp("cp", c[0]) == 0) {

		cp(c[2], c[3]);

	}
	else if (strcmp("chmod", c[0]) == 0) {
		int status;
		num = 3;
		cnt = 3;
		while (strcmp(c[cnt], "") != 0) cnt++;	////파라미터로 전달받은 폴더 갯수 카운트. 폴더는 2부터 시작함(c[2])
		pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 3));	////만드는 폴더의 갯수만큼 스레드 생성
		for (int i = 0; i < cnt - 3; i++)
		{
			if ((status = pthread_create(&tid[i], NULL, &chmodthrfunc, NULL)) != 0)		////thrd 진입
			{
				printf("error occured in creating thread!!");
				exit(0);
			}
		}
		for (i = 0; i < cnt - 3; i++) pthread_join(tid[i], NULL);			////thrd종료 대기
		free(tid);////thread free
	}
	else if (strcmp("find", c[0]) == 0) {
		if (strcmp(c[3], "") != 0) {
			int len = strlen(c[3]);
			c[3][len - 1] = '\0';
			strcpy(c[3], &c[3][1]);
		}
		find(c[1], c[2], c[3]);
	}
	else if (strcmp("cat", c[0]) == 0) {
		if (strcmp(c[1], ">") == 0) cat(c[1], c[2]);			////cat >로 파일을 생성할때는 멀티스레딩 안됨.
		else												////else는 가능
		{
			linecnt = 1;		////line갯수 세어주는 전용 전역변수 초기화 아래는 위와 논리가 같음
			num = 2;
			cnt = 2;
			int status;
			while (strcmp(c[cnt], "") != 0) cnt++;
			pthread_t *tid; tid = (pthread_t*)malloc(sizeof(pthread_t)*(cnt - 2));
			for (int i = 0; i < cnt - 2; i++)
			{
				if ((status = pthread_create(&tid[i], NULL, &catthrfunc, NULL)) != 0)
				{
					printf("error occured in creating thread!!");
					exit(0);
				}
			}
			for (i = 0; i < cnt - 2; i++) pthread_join(tid[i], NULL);
			free(tid);
		}
	}
	else if (strcmp("exit", c[0]) == 0) {
		setEmpty();
		push("%");
		save(root, root);
		return false;
	}
	else
		printf("!!wrong command please check again.!!\n");
	return true;
}

void *mkdirthrfunc(void *arg)
{
	pthread_mutex_lock(&lock);
	if (strstr(c[1], "m") != 0)_makeDir(cur, c[num], c[1], atoi(c[2]), true);
	else _makeDir(cur, c[num], c[1], 0, true);	////한 스레드마다 mkdir할수있는 함수
	num++;
	sleep(1);						////deadlock을 방지해서 1ms정도 sleep
	pthread_mutex_unlock(&lock);
	sleep(1);
	return NULL;
}
void *rmthrfunc(void *arg)	////위와 같은 논리
{
	pthread_mutex_lock(&lock);
	rm(c[1], c[num]);
	num++;
	sleep(1);
	pthread_mutex_unlock(&lock);
	sleep(1);
	return NULL;

}
void *catthrfunc(void *arg)	////위와 같은 논리
{
	pthread_mutex_lock(&lock);
	cat(c[1], c[num]);
	num++;
	sleep(1);
	pthread_mutex_unlock(&lock);
	sleep(1);
	return NULL;

}

void save(ptr_lcrs now, ptr_lcrs pare)
{
	if (now == NULL)return;
	if (now != pare) {
		int len = strlen(now->name);
		char *c; c = (char*)malloc(sizeof(char)*(len + 3));
		memset(c, 0, sizeof(c));
		if (st_num != 0)c[0] = '%';
		if ((now->name)[0] != '.')strcat(c, now->name);      //스택 수정
		else {
			for (int i = 0 + (st_num != 0); i < len + (st_num != 0); i++)c[i] = '=';
		}
		push(c);
		free(c);
	}
	save_module(now);
	if ((now->name)[0] != '.')
		save(now->child, pare);
	if (now != pare)
	{
		int lenn = strlen(now->name);
		if (st_num - lenn > 0) lenn++;
		pop(lenn);
	}
	save(now->sib, pare);
}
void save_module(ptr_lcrs now)
{
	if (now != NULL)
	{
		FILE *opfile = fopen(st, "w");
		if (opfile != NULL)
		{
			struct tm *date = now->t;
			fprintf(opfile, "%s\n", now->name);
			fprintf(opfile,
				"%d %d %d %d %d %d\n",
				date->tm_year, date->tm_mon, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec
			);
			fprintf(opfile, "%d\n", now->chmod);
			fprintf(opfile, "%d %d\n", now->hide, now->type);
			if (now->type)
				fprintf(opfile, "%s\t\n", now->contents);
			else
			{
				fprintf(opfile, "%s\n", (now->child != NULL) ? now->child->name : "...");
			}
			fprintf(opfile, "%s\n", (now->sib != NULL) ? now->sib->name : "...");
			fclose(opfile);
		}
	}
}

ptr_lcrs load_module(ptr_lcrs now, ptr_lcrs pare)
{
	FILE *fp = fopen(st, "r");
	if (fp != NULL)
	{
		char buf[50];
		if (now == NULL)
			now = (ptr_lcrs)malloc(sizeof(lcrs));
		now->contents = NULL;
		if (st_max != 0)
		{
			fscanf(fp, "%s", buf);
			now->name = (char *)malloc(sizeof(char)*(strlen(buf) + 3));
			strcpy(now->name, buf);
			if (buf[0] != '.') { now->child = NULL; now->sib = NULL; }
		}
		else
		{
			now->name = (char *)malloc(sizeof(char) * 2);
			strcpy(now->name, "");
		}
		struct tm *date = (struct tm*)malloc(sizeof(struct tm));
		fscanf(fp, "%d %d %d %d %d %d",
			&(date->tm_year), &(date->tm_mon), &(date->tm_mday), &(date->tm_hour), &(date->tm_min), &(date->tm_sec)
		);
		now->t = date;
		fscanf(fp, "%d", &(now->chmod));
		fscanf(fp, "%d", &(now->hide));
		if (now->name[0] != '.') {
			fscanf(fp, "%d", &(now->type));
		}
		else {
			fscanf(fp, "%d", buf);
		}

		if (now->type)
		{
			fscanf(fp, "%[^'\t']", buf);
			char *token = strstr(buf, "\n");
			strcpy(buf, token + 1);
			now->contents = (char *)malloc(sizeof(char)*(strlen(buf) + 3));
			strcpy(now->contents, buf);
		}
		else {
			if ((now->name)[0] != '.')
			{
				now->child = NULL;
				now->child = (ptr_lcrs)malloc(sizeof(lcrs));
				now->child->type = false;
				now->child->child = now;
				now->child->sib = NULL;
				now->child->sib = (ptr_lcrs)malloc(sizeof(lcrs));
				now->child->sib->hide = false;
				now->child->sib->child = pare;
			}
			fscanf(fp, "%s", buf);
			if (strcmp(buf, "...") != 0 && (now->name)[0] != '.')//child 구성
			{
				int len = strlen(buf);   //자식이름
				char *c; c = (char*)malloc(sizeof(char)*(len + 3));
				memset(c, 0, sizeof(c));
				if (st_num != 0)c[0] = '%';
				int gap = (st_num != 0) ? 1 : 0;
				if (buf[0] != '.')strcat(c, buf);      //스택 수정
				else {
					for (int i = 0 + gap; i < len + gap; i++)c[i] = '=';
				}
				push(c);
				free(c);
				now->child = load_module(now->child, now);
			}
			int len = strlen(now->name);
			if (st_num - len > 0)len++;
			pop(len);
		}
		if (now->type) setEmpty();
		fscanf(fp, "%s", buf);
		if (strcmp(now->name, ".") != 0)now->sib = NULL;
		if (strcmp(buf, "...") != 0)
		{
			int len = strlen(buf);
			char *c; c = (char*)malloc(sizeof(char)*(len + 3));
			memset(c, 0, sizeof(c));
			int gap = (st_num != 0) ? 1 : 0;
			if (st_num != 0)c[0] = '%';
			if (buf[0] != '.')strcat(c, buf);      //스택 수정
			else {
				for (int i = 0 + gap; i < len + gap; i++)c[i] = '=';
			}
			push(c);
			free(c);
			now->sib = load_module(now->sib, pare);
		}
		fclose(fp);
		return now;
	}
	return NULL;
}
void *chmodthrfunc(void *arg)
{
	pthread_mutex_lock(&lock);
	chmod(atoi(c[2]), c[num]);
	num++;
	sleep(1);
	pthread_mutex_unlock(&lock);
	sleep(1);
	return NULL;
}

void *touchthrfunc(void *arg)
{
	pthread_mutex_lock(&lock);
	touch(c[num]);
	num++;
	sleep(1);
	pthread_mutex_unlock(&lock);
	sleep(1);
	return NULL;
}