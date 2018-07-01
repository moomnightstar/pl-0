// pl0 compiler source code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pl0.h"
#include "set.c"

//////////////////////////////////////////////////////////////////////
// print error message.
void error(int n)
{
	int i;

	printf("      ");
	for (i = 1; i <= cc - 1; i++)
		printf(" ");
	printf("^\n");
	printf("Error %3d: %s\n", n, err_msg[n]);
	err++;
	exit(1);
} // error

//////////////////////////////////////////////////////////////////////
void getch(void)
{
	if (cc == ll)
	{
		if (feof(infile))
		{
			printf("\nPROGRAM INCOMPLETE\n");
			exit(1);
		}
		ll = cc = 0;
		printf("%5d  ", cx);
		while ( (!feof(infile)) // added & modified by alex 01-02-09
			    && ((ch = getc(infile)) != '\n'))
		{
			printf("%c", ch);
			line[++ll] = ch;
		} // while
		printf("\n");
		line[++ll] = '\n'; //  更改过的行缓冲储存回车\n
		line[++ll] = ' ';
	}
	ch = line[++cc];
} // getch

//////////////////////////////////////////////////////////////////////
// gets a symbol from input stream.
void getsym(void)
{
	int i, k;
	char a[MAXIDLEN + 1];

	while ((ch == ' ')||(ch=='\n') ||(ch=='\t')){  //已更改getch的行缓冲，使其可以存储回车。这里跳过空格、制表符、回车
		getch();
  }
	if (isalpha(ch))
	{ // symbol is a reserved word or an identifier.
		k = 0;
		do
		{
			if (k < MAXIDLEN)
				a[k++] = ch;
			getch();
		}
		while (isalpha(ch) || isdigit(ch));
		a[k] = 0;
		strcpy(id, a);
		word[0] = id;
		i = NRW;
		while (strcmp(id, word[i--]));
		if (++i)
			sym = wsym[i]; // symbol is a reserved word
		else
			sym = SYM_IDENTIFIER;   // symbol is an identifier
	}
	else if (isdigit(ch))
	{ // symbol is a number.
		k = num = 0;
		sym = SYM_NUMBER;
		do
		{
			num = num * 10 + ch - '0';
			k++;
			getch();
		}
		while (isdigit(ch));
		if (k > MAXNUMLEN)
			error(25);     // The number is too great.
	}
	else if (ch == ':')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_BECOMES; // :=
			getch();
		}
		else
		{
			sym = SYM_NULL;       // illegal?
		}
	}
	else if (ch == '>')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_GEQ;     // >=
			getch();
		}
		else
		{
			sym = SYM_GTR;     // >
		}
	}
	else if (ch == '<')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_LEQ;     // <=
			getch();
		}
		else if (ch == '>')
		{
			sym = SYM_NEQ;     // <>
			getch();
		}
		else
		{
			sym = SYM_LES;     // <
		}
	}
	else if (ch == '&') {  //添加&&和||,!在csym中
		getch();
		if (ch == '&') {
			sym = SYM_AND;
			getch();
		}
		//else保留可做位运算或error
	}
	else if (ch == '|') {
		getch();
		if (ch == '|') {
			sym = SYM_OR;
			getch();
		}
		//else保留可做位运算或error
	}
	else if (ch == '/') {   //以下代码为添加检测行注释和快注释的代码。
		getch();
		if (ch == '/') {    //  添加//
			getch();			//读下一个字符
			while (ch != '\n') {  //若是不是回车,则跳过。直到读到\n,结束循环
				getch();
			}
		  getch();  //读下一个字符
			getsym();  /*考虑到getsym函数只是得到sym的值，没有返回值。
					   在调用getsym读完注释后，没有获得sym值。则可以递归调用getsym，得到下一串字符代表得sym。
						若下一串仍为注释，则会继续递归调用getsym。直到获得一个sym。*/
		}
		else if (ch == '*') {		// 添加 /*  */，不允许嵌套
			int flag = 1;          //设立标志，
			while (flag > 0) {    //flag为1时，代表仍在注释块中
				getch();        
				while (ch != '*') {  //跳过不是*的字符
					getch();
				}
				getch();     //若已读到*,则读下一个字符，判读下一个字符是否是/
				if (ch == '/')	flag = 0; //若是/,flag置0,代表已读完注释块，跳出循环；若不是，循环继续
				//后续可再添加 error：若读到文件末尾，仍没有*/与之配对
			}
			getch();  //读下一个字符，同上面一样递归调用getsym,直到获得一个sym
			getsym();  	
		}
		//else   保留可添加其它由/开头的标志
	}
	else
	{ // other tokens
		i = NSYM;
		csym[0] = ch;
		while (csym[i--] != ch);
		if (++i)
		{
			sym = ssym[i];
			getch();
		}
		else
		{
			printf("Fatal Error: Unknown character.\n");
			exit(1);
		}
	}
} // getsym

//////////////////////////////////////////////////////////////////////
// generates (assembles) an instruction.
void gen(int x, int y, int z)
{
	if (cx > CXMAX)
	{
		printf("Fatal Error: Program too long.\n");
		exit(1);
	}
	code[cx].f = x;
	code[cx].l = y;
	code[cx++].a = z;
} // gen

//////////////////////////////////////////////////////////////////////
// tests if error occurs and skips all symbols that do not belongs to s1 or s2.
void test(symset s1, symset s2, int n)
{
	symset s;

	if (! inset(sym, s1))
	{
		error(n);
		//printf("%d", sym);
		s = uniteset(s1, s2);
		while(! inset(sym, s))
			getsym();
		destroyset(s);
	}
} // test

//////////////////////////////////////////////////////////////////////
int dx;  // data allocation index

// enter object(constant, variable or procedre) into table.
void enter(int kind)
{
	mask* mk;
	switch (kind)
	{
	case ID_CONSTANT:
		tx++;
		strcpy(table[tx].name, id);
		table[tx].kind = kind;
		if (num > MAXADDRESS)
		{
			error(25); // The number is too great.
			num = 0;
		}
		table[tx].value = num;
		break;
	case ID_VARIABLE:
		getsym();
		if (sym == SYM_LEPA) {  //是数组时
			int cnt = 0;
			while (sym == SYM_LEPA) {
				getsym();
				if (num < 1)  error(34);//数组维数必须为正
				if (num > MAXADDRESS)
				{
					error(25); // The number is too great.
					num = 0;
				}
				tx++;
				strcpy(table[tx].name, "#");  //存入table表中
				table[tx].kind =ID_CONSTANT;
				table[tx].value = num;   
				arraysize *= num;   //获得数组空间大小
				getsym();
				if (sym != SYM_REPA)//维数后的应该是‘]’
				{
					error(33);//如果没有‘]’报错     
				}
				cnt++;
				getsym();  //;号
			}
			tx++;
			strcpy(table[tx].name, "#");
			table[tx].kind = ID_CONSTANT;
			table[tx].kind = ID_CONSTANT;
			table[tx].value = cnt;  //存数组维数
			for (int i = 0; i < arraysize; i++) {
				tx++;
				strcpy(table[tx].name, id);
				table[tx].kind = kind;
				mk = (mask*)&table[tx];
				mk->level = level;
				mk->address = dx++;
			}
			arraysize = 1;
		}
		else { //一般变量
			tx++;
			strcpy(table[tx].name, id);
			table[tx].kind = kind;
			mk = (mask*)&table[tx];
			mk->level = level;
			mk->address = dx++;
		}
	case ID_PROCEDURE:
		tx++;
		strcpy(table[tx].name, id);
		table[tx].kind = kind;
		mk = (mask*) &table[tx];
		mk->level = level;
		break;
	} // switch
} // enter

//////////////////////////////////////////////////////////////////////
// locates identifier in symbol table.
int position(char* id)
{
	int i;
	strcpy(table[0].name, id);
	i = tx + 1;
	while (strcmp(table[--i].name, id) != 0);
	return i;
} // position

//////////////////////////////////////////////////////////////////////
void constdeclaration()
{
	if (sym == SYM_IDENTIFIER)
	{
		getsym();
		if (sym == SYM_EQU || sym == SYM_BECOMES)
		{
			if (sym == SYM_BECOMES)
				error(1); // Found ':=' when expecting '='.
			getsym();
			if (sym == SYM_NUMBER)
			{
				enter(ID_CONSTANT);
				getsym();
			}
			else
			{
				error(2); // There must be a number to follow '='.
			}
		}
		else
		{
			error(3); // There must be an '=' to follow the identifier.
		}
	} else	error(4);
	 // There must be an identifier to follow 'const', 'var', or 'procedure'.
} // constdeclaration

//////////////////////////////////////////////////////////////////////
void vardeclaration(void)
{
	if (sym == SYM_IDENTIFIER)
	{
		enter(ID_VARIABLE);
		//getsym();
	}
	else
	{
		error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
	}
} // vardeclaration

//////////////////////////////////////////////////////////////////////
void listcode(int from, int to)
{
	int i;
	
	printf("\n");
	for (i = from; i < to; i++)
	{
		printf("%5d %s\t%d\t%d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
	}
	printf("\n");
} // listcode

//////////////////////////////////////////////////////////////////////
void factor(symset fsys)
{
	void expression(symset fsys);
	int i;
	symset set,set1;
	int relop=0;
	test(facbegsys, fsys, 24); // The symbol can not be as the beginning of an expression.
	if (sym == SYM_NOT || sym == SYM_ODD) {
		relop = sym;
		getsym();
	}
	while (inset(sym, facbegsys))
	{
		if (sym == SYM_IDENTIFIER)
		{
			if ((i = position(id)) == 0)
			{
				error(11); // Undeclared identifier.
			}
			else
			{
				switch (table[i].kind)
				{
					mask* mk;
				case ID_CONSTANT:
					gen(LIT, 0, table[i].value);
					getsym();
					break;
				case ID_VARIABLE:
					getsym();
					if (sym == SYM_LEPA) { //[
						while (sym == SYM_LEPA) {
							getsym();
							set1 = createset(SYM_LEPA, SYM_REPA, SYM_NULL);
							set = uniteset(fsys, set1);
							expression(set);
							destroyset(set1);
							destroyset(set);
							if (sym == SYM_REPA)//下标后的应该是‘]’
							{
								getsym();
							}
							else
							{
								error(33);//如果没有‘]’报错     
							}
						}
						for (int j = table[i-1].value; j >=0; j--) {
							gen(LIT, 0, table[i - 1 - j].value);   //将维数，维度大小存入stack
						}
						mk = (mask*)&table[i];
						gen(LAD, level - mk->level, mk->address);
					}
					else {
						mk = (mask*)&table[i];
						gen(LOD, level - mk->level, mk->address);
					}
					break;
				case ID_PROCEDURE:
					error(21); // Procedure identifier can not be in an expression.
					getsym();
					break;
				} // switch
			}
			//getsym();
		}
		else if (sym == SYM_NUMBER)
		{
			if (num > MAXADDRESS)
			{
				error(25); // The number is too great.
				num = 0;
			}
			gen(LIT, 0, num);
			getsym();
		}
		else if (sym == SYM_LPAREN)   //(
		{
			getsym();
			set = uniteset(createset(SYM_RPAREN, SYM_NULL), fsys);
			expression(set);
			destroyset(set);
			if (sym == SYM_RPAREN)
			{
				getsym();
			}
			else
			{
				error(22); // Missing ')'.
			}
		}
		test(fsys, createset(SYM_LPAREN, SYM_NULL), 23);
	} // while
	if (relop != 0) {
		switch (relop) {
		case SYM_NOT: gen(OPR, 0, OPR_NOT); break;
		case SYM_ODD: gen(OPR, 0, OPR_NOT); break;
		}
	}
} // factor

//////////////////////////////////////////////////////////////////////
void term(symset fsys)
{
	int mulop;
	symset set;
	
	set = uniteset(fsys, createset(SYM_TIMES, SYM_SLASH, SYM_NULL));  //SLASH 斜杠 / 除法
	factor(set);
	while (sym == SYM_TIMES || sym == SYM_SLASH)
	{
		mulop = sym;
		getsym();
		factor(set);
		if (mulop == SYM_TIMES)
		{
			gen(OPR, 0, OPR_MUL);
		}
		else
		{
			gen(OPR, 0, OPR_DIV);
		}
	} // while
	destroyset(set);
} // term

//////////////////////////////////////////////////////////////////////
void expression(symset fsys)
{
	int addop;
	symset set;

	set = uniteset(fsys, createset(SYM_PLUS, SYM_MINUS, SYM_NULL));
	if (sym == SYM_PLUS || sym == SYM_MINUS)   
	{
		addop = sym;
		getsym();
		term(set);
		if (addop == SYM_MINUS)
		{
			gen(OPR, 0, OPR_NEG);
		}
		
	}
	else
	{
		term(set);
	}

	while (sym == SYM_PLUS || sym == SYM_MINUS )
	{
		addop = sym;
		getsym();
		term(set);
		if (addop == SYM_PLUS)
		{
			gen(OPR, 0, OPR_ADD);
		}
		else
		{
			gen(OPR, 0, OPR_MIN);
		}
	} // while
	destroyset(set);
} // expression

//////////////////////////////////////////////////////////////////////
void condition(symset fsys)
{
	int relop;
	symset set;
	set = uniteset(relset, fsys);
	expression(set);
	destroyset(set);
		if (!inset(sym, relset)) //若sym不是比较运算
		{
			//error(20);
			//printf("比较运算结束或纯表达式\n"); 
		}
		else
		{
			relop = sym;
			getsym();
			expression(fsys);
			
			switch (relop)
			{
			case SYM_EQU:
				gen(OPR, 0, OPR_EQU);
				break;
			case SYM_NEQ:
				gen(OPR, 0, OPR_NEQ);
				break;
			case SYM_LES:
				gen(OPR, 0, OPR_LES);
				break;
			case SYM_GEQ:
				gen(OPR, 0, OPR_GEQ);
				break;
			case SYM_GTR:
				gen(OPR, 0, OPR_GTR);
				break;
			case SYM_LEQ:
				gen(OPR, 0, OPR_LEQ);
				break;
			} // switch
		} // else
} // condition


//////////////////////////////////////////////////////////////////////
void statement(symset fsys)
{
	int i, cx1, cx2,cx3,cx4,cnt=0;
	mask* mk[TXMAX];
	symset set1, set;

	if (sym == SYM_IDENTIFIER)
	{ // variable assignment
		mask* mk;
		if (! (i = position(id)))
		{
			error(11); // Undeclared identifier.
		}
		else if (table[i].kind != ID_VARIABLE)
		{
			error(12); // Illegal assignment.
			i = 0;
		}
		getsym();
		if (sym == SYM_LEPA) { //[
			while (sym == SYM_LEPA) {
				getsym();
				set1 = createset(SYM_LEPA, SYM_REPA, SYM_NULL);
				set = uniteset(fsys, set1);
				expression(set);
				destroyset(set1);
				destroyset(set);
				if (sym == SYM_REPA)//下标后的应该是‘]’
				{
					getsym();
				}
				else
				{
					error(33);//如果没有‘]’报错     
				}
			}
			for (int j = table[i - 1].value; j >= 0;j--) {
				gen(LIT, 0, table[i - 1-j].value);   //将维数，维度大小存入stack
			}
			if (sym == SYM_BECOMES)
			{
				getsym();
			}
			else
			{
				error(13); // ':=' expected.
			}
			expression(fsys);
			mk = (mask*)&table[i];
			gen(ATO, level - mk->level, mk->address);
		}
		else {
			if (sym == SYM_BECOMES)
			{
				getsym();
			}
			else
			{
				error(13); // ':=' expected.
			}
			expression(fsys);
			mk = (mask*)&table[i];
			if (i)
			{
				gen(STO, level - mk->level, mk->address);
			}
		}
	}
	else if (sym == SYM_CALL)
	{ // procedure call
		getsym();
		if (sym != SYM_IDENTIFIER)
		{
			error(14); // There must be an identifier to follow the 'call'.
		}
		else
		{
			if (! (i = position(id)))
			{
				error(11); // Undeclared identifier.
			}
			else if (table[i].kind == ID_PROCEDURE)
			{
				mask* mk;
				mk = (mask*) &table[i];
				gen(CAL, level - mk->level, mk->address);
			}
			else
			{
				error(15); // A constant or variable can not be called. 
			}
			getsym();
		}
	} 
	else if (sym == SYM_IF)       //条件语句指令。if then  ... else采用最近匹配原则，外层if可以没有else。
	{ // if statement
		int j,t,f,truecx[CXMAX], falsecx[CXMAX];  //真假跳转地址
		t = 0; f = 0;
		getsym();
		set1 = createset(SYM_THEN, SYM_DO, SYM_AND, SYM_OR, SYM_NULL);
		set = uniteset(set1, fsys);
			// 单目逻辑运算,在因子级别运算;让逻辑运算与比较运算分开来以实现其优先级划分 
			condition(set);   //第一个逻辑元运算
			while (sym == SYM_AND||sym==SYM_OR) { //若逻辑运算没有结束
				if (sym == SYM_AND) {
					falsecx[f] = cx;
					gen(JPC, 0, 0);
					f++;
				}
				if (sym ==SYM_OR) {
					truecx[t] = cx;
					gen(JPX, 0, 0);
					t++;
				}
					
					getsym();
					condition(set);
				/*	switch (sym)
					{
					case SYM_AND:				//添加逻辑指令
						gen(OPR, 0, OPR_AND);
						break;
					case SYM_OR:
						gen(OPR, 0, OPR_OR);
						break;
					}//switch	*/
			}//while
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_THEN)    //if后必须要有then 
		{
			getsym();
		}
		else
		{
			error(16); // 'then' expected.
		}
		cx1 = cx;
		gen(JPC, 0, 0);    //设置if之后的条件跳转指令—JPC，跳转地址未定，采用回填技术待会填入。
		for (j = 0; j < t; j++) {
			code[truecx[j]].a = cx;
		}
		statement(fsys);
		cx2 = cx;
		gen(JMP, 0, cx+1);//设置then之后的无条件跳转指令—JMP。若无else,默认跳转为then之后的一条语句，若有else,再采用回填技术填入  
		for(j = 0; j < f; j++){
			code[falsecx[j]].a = cx;  //填入假链跳转地址
		}
		code[cx1].a = cx;	//填入cx1时的跳转地址，即JPC跳转到else语句
		if (sym == SYM_SEMICOLON) getsym();
		if(sym==SYM_ELSE)  //添加else指令
		{
			getsym();
			statement(fsys);
			code[cx2].a = cx;//填入cx2时的跳转地址,即then语句结束之后，无条件跳转JMP到else语句段之后的一条语句（跳过else段）
		}
		else {
			statement(fsys);
		}
		
	}
	else if (sym == SYM_BEGIN)
	{ // block
		getsym();
		set1 = createset(SYM_SEMICOLON, SYM_END, SYM_NULL);
		set = uniteset(set1, fsys);
		statement(set);
		while (sym == SYM_SEMICOLON || inset(sym, statbegsys))
		{
			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(10);
			}
			statement(set);
		} // while
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_END)
		{
			getsym();
		}
		else
		{
			error(17); // ';' or 'end' expected.
		}
	}
	else if (sym == SYM_EXIT)	//exit
	{
		exitcx[exitnum] = cx;
		exitnum++;
		gen(JMP, 0, 0);
		getsym();
	}
	else if (sym == SYM_WHILE)
	{ // while statement
		cx1 = cx;
		getsym();
		set1 = createset(SYM_DO, SYM_NULL);
		set = uniteset(set1, fsys);
		condition(set);
		destroyset(set1);
		destroyset(set);
		cx2 = cx;
		cnt = bp;
		breakmem[bp] = cx;  //此处把跳转到出口语句的地址记录
		bp++;
		gen(JPC, 0, 0);
		if (sym == SYM_DO)
		{
			getsym();
		}
		else
		{
			error(18); // 'do' expected.
		}
		++breakdepth;
		statement(fsys);
		--breakdepth;
		gen(JMP, 0, cx1);
		code[cx2].a = cx;
		while (bp>cnt)
		{
			bp--;
			code[breakmem[bp]].a = cx;//break依次回填出口 
		}
	}
	else if (sym == SYM_FOR)  //for 
	{
		set1 = createset(SYM_SEMICOLON, SYM_NULL);//读分号之前set 
		set = uniteset(set1, fsys);//与fsys连接 
		getsym();
		if (sym == SYM_LPAREN)//左括号 
		{
			getsym();
		}
		else
		{
			error(28);
		}
		if (sym == SYM_SEMICOLON) //分号可以跳过 
		{
			getsym();
		}
		else
		{
			statement(set);//赋值语句为statement调用自身 
			if (sym != SYM_SEMICOLON)//need 分号 
			{
				error(10);
			}
			else
			{
				getsym();
			}
		}
		if (sym == SYM_SEMICOLON)
		{
			cx1 = cx;
			getsym();
			forflag = 1;//forflag,用于判断第二个condition语句是否为空。
		}
		else
		{//condition语句 
			cx1 = cx;
			condition(set);
			destroyset(set1);
			destroyset(set);
			if (sym != SYM_SEMICOLON)
			{
				error(10);
			}
			else
			{
				getsym();
			}
			cx2 = cx;
			cnt = bp;
			breakmem[bp] = cx;  //此处把跳转到出口语句的地址记录
			bp++;
			gen(JPC, 0, 0);//条件跳转出口，地址稍后回填，break的jmp地址指向这里 
		}
		cx4 = cx;  //第三项开始 
		gen(JMP, 0, 0);
		if (sym == SYM_RPAREN)//第三项为空 
		{
			cx3 = cx;
			getsym();
			gen(JMP, 0, cx1);
			code[cx4].a = cx;
		}
		else
		{
			cx3 = cx;
			set1 = createset(SYM_RPAREN, SYM_NULL);
			set = uniteset(set1, fsys);
			statement(set);
			destroyset(set1);
			destroyset(set);
			if (sym != SYM_RPAREN)
			{
				error(22);
			}
			else
			{
				getsym();
			}
			gen(JMP, 0, cx1);   //跳到判断语句处 
			code[cx4].a = cx;  //回填地址 
		}

		set1 = createset(SYM_RPAREN, SYM_NULL);
		set = uniteset(set1, fsys);
		++breakdepth;
		statement(set);
		--breakdepth;
		destroyset(set1);
		destroyset(set);
		gen(JMP, 0, cx3);   //do代码段结束，跳至for第三段 
		if (forflag == 0)//条件跳转出口 
		{
			code[cx2].a = cx;
			forflag = 1;
		}
		while (bp>cnt)
		{
			bp--;
			code[breakmem[bp]].a = cx;//break依次回填出口 
		}
	}
	else if (sym == SYM_BREAK)
	{
		if (breakdepth <= 0)
		{
			error(27);
		}
		else
		{
			breakmem[bp] = cx;	//当前地址保存 
			bp++;
			gen(JMP, 0, 0);		//生成跳转指令，稍后回填 
			getsym();
		}
	}
	else if (sym == SYM_READ)
	{
		getsym();
		if (sym != SYM_LPAREN)  //read需要左括号 
		{
			error(28);
		}
		else
		{
			do
			{
				getsym();
				if (sym == SYM_IDENTIFIER)
				{
					i = position(id);
				}
				else i = 0;
				if (i == 0)
				{
					error(31);
				}//判断a是否为一个存在的变量 
				else
				{
					mk[i] = (mask*)&table[i];
					gen(OPR, 0, OPR_READ);	//调用scanf 
					gen(STO, level - mk[i]->level, mk[i]->address);//变量存入空间 
				}
				getsym();
			} while (sym == SYM_COMMA);	//多个变量由逗号分隔 
		}
		if (sym != SYM_RPAREN)  //右括号结束 
		{
			error(22);
			while (!inset(sym, fsys)) //找到下一个合法token 
				getsym();
		}
		else
		{
			getsym();
		}
	}
	else if (sym == SYM_WRITE)  //与read类似 
	{
		getsym();
		if (sym == SYM_LPAREN)
		{
			do
			{
				getsym();
				set1 = createset(SYM_RPAREN, SYM_COMMA, SYM_NULL); //获得表达式调用exp计算 
				set = uniteset(set1, fsys);
				expression(set);
				gen(OPR, 0, OPR_PRINT);
			} while (sym == SYM_COMMA); //同样是逗号分隔 
			if (sym != SYM_RPAREN)
			{
				error(7);
			}
			else
			{
				getsym();
			}
			gen(OPR, 0, OPR_LINEEND); //输出一个换行 
		}
	}
	test(fsys, phi, 19);
} // statement
			
//////////////////////////////////////////////////////////////////////
void block(symset fsys)
{
	int cx0; // initial code index
	mask* mk;
	int block_dx;
	int savedTx;
	symset set1, set;

	dx = 3;
	block_dx = dx;
	mk = (mask*) &table[tx];
	mk->address = cx;
	gen(JMP, 0, 0);
	if (level > MAXLEVEL)
	{
		error(32); // There are too many levels.
	}
	do
	{
		if (sym == SYM_CONST)
		{ // constant declarations
			getsym();
			do
			{
				constdeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					constdeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
		} // if

		if (sym == SYM_VAR)
		{ // variable declarations
			getsym();
			do
			{
				vardeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					vardeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
//			block = dx;
		} // if

		while (sym == SYM_PROCEDURE)
		{ // procedure declarations
			getsym();
			if (sym == SYM_IDENTIFIER)
			{
				enter(ID_PROCEDURE);
				getsym();
			}
			else
			{
				error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
			}


			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}

			level++;
			savedTx = tx;
			set1 = createset(SYM_SEMICOLON, SYM_NULL);
			set = uniteset(set1, fsys);
			block(set);
			destroyset(set1);
			destroyset(set);
			tx = savedTx;
			level--;

			if (sym == SYM_SEMICOLON)
			{
				getsym();
				set1 = createset(SYM_IDENTIFIER, SYM_PROCEDURE, SYM_NULL);
				set = uniteset(statbegsys, set1);
				test(set, fsys, 6);
				destroyset(set1);
				destroyset(set);
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}
		} // while
		set1 = createset(SYM_IDENTIFIER, SYM_NULL);
		set = uniteset(statbegsys, set1);
		test(set, declbegsys, 7);
		destroyset(set1);
		destroyset(set);
	}
	while (inset(sym, declbegsys));

	code[mk->address].a = cx;
	mk->address = cx;
	cx0 = cx;
	gen(INT, 0, block_dx);
	set1 = createset(SYM_SEMICOLON, SYM_END, SYM_NULL);
	set = uniteset(set1, fsys);
	statement(set);
	destroyset(set1);
	destroyset(set);

	for (int i = 0; i < exitnum; i++) {  //exit
		code[exitcx[i]].a = cx; exitcx[i] = 0; //回填所有的exit跳转地址
	}
	exitnum = 0;
	gen(OPR, 0, OPR_RET); // return
	test(fsys, phi, 8); // test for error: Follow the statement is an incorrect symbol.
	listcode(cx0, cx);
} // block

//////////////////////////////////////////////////////////////////////
int base(int stack[], int currentLevel, int levelDiff)
{
	int b = currentLevel;
	
	while (levelDiff--)
		b = stack[b];
	return b;
} // base

//////////////////////////////////////////////////////////////////////
// interprets and executes codes.
void interpret()
{
	int pc;        // program counter
	int stack[STACKSIZE];
	int top;       // top of stack
	int b;         // program, base, and top-stack register
	instruction i; // instruction register
	int cnt, temp = 1, address = 0;
	printf("Begin executing PL/0 program.\n");

	pc = 0;
	b = 1;
	top = 3;
	stack[1] = stack[2] = stack[3] = 0;
	do
	{
		i = code[pc++];
		switch (i.f)
		{
		case LIT:
			stack[++top] = i.a;
			break;
		case OPR:
			switch (i.a) // operator
			{
			case OPR_RET:
				top = b - 1;
				pc = stack[top + 3];
				b = stack[top + 2];
				break;
			case OPR_NEG:
				stack[top] = -stack[top];
				break;
			case OPR_ADD:
				top--;
				stack[top] += stack[top + 1];
				break;
			case OPR_MIN:
				top--;
				stack[top] -= stack[top + 1];
				break;
			case OPR_MUL:
				top--;
				stack[top] *= stack[top + 1];
				break;
			case OPR_DIV:
				top--;
				if (stack[top + 1] == 0)
				{
					fprintf(stderr, "Runtime Error: Divided by zero.\n");
					fprintf(stderr, "Program terminated.\n");
					continue;
				}
				stack[top] /= stack[top + 1];
				break;
			case OPR_ODD:
				stack[top] %= 2;
				break;
			case OPR_EQU:
				top--;
				stack[top] = stack[top] == stack[top + 1];
				break;
			case OPR_NEQ:
				top--;
				stack[top] = stack[top] != stack[top + 1];
			case OPR_LES:
				top--;
				stack[top] = stack[top] < stack[top + 1];
				break;
			case OPR_GEQ:
				top--;
				stack[top] = stack[top] >= stack[top + 1];
			case OPR_GTR:
				top--;
				stack[top] = stack[top] > stack[top + 1];
				break;
			case OPR_LEQ:
				top--;
				stack[top] = stack[top] <= stack[top + 1];
				break;
			case OPR_AND:
				top--;
				stack[top] = stack[top] && stack[top + 1];  //添加and运算，这里用了隐式逻辑转换
				break;
			case OPR_OR:
				top--;
				stack[top] = stack[top] || stack[top + 1];
				break;
			case OPR_NOT:
				stack[top] = !stack[top];
				break;
			case OPR_READ: //新增指令read,读一个数 
				top++;
				scanf("%d", &stack[top]);
				break;
			case OPR_LINEEND: //新增指令换行 
				printf("\n");
				break;
			case OPR_PRINT:  //新增指令打印 
				printf("%d", stack[top]);
				top--;
				break;
			} // switch
			break;
		case LOD:
			stack[++top] = stack[base(stack, b, i.l) + i.a];
			break;
		case LAD:
			cnt = stack[top];
			for (int j = 1; j <= cnt; j++) {
				address += stack[top - cnt - j ] * temp; printf("%d ", stack[top - cnt  - 1]);
				temp *= stack[top - j ]; printf("%d ", stack[top - j ]);
			}
			printf("cnt=%d,top=%d\n", cnt, top);
			scanf("%d", &temp);
			top = top - 2*cnt;
			stack[top] = stack[base(stack, b, i.l) + i.a+address];
			address = 0;
			temp = 1;
			break;
		case STO:
			stack[base(stack, b, i.l) + i.a] = stack[top];
			//printf("level=%d\n", level);
			printf("%d\n", stack[top]);
			top--;
			break;
		case ATO://添加数组赋值语句
			cnt = stack[top - 1];
			for (int j = 1; j <= cnt; j++) {
				address += stack[top - cnt-j-1] * temp; 
				temp *= stack[top - j - 1]; 
			}
			stack[base(stack, b, i.l) + i.a+address] = stack[top];
			printf("%d\n", stack[top]);
			printf("cnt=%d,top=%d\n", cnt, top);
			scanf("%d", &temp);
			top=top-2*cnt-2;
			address = 0;
			temp = 1;
			break;
		case CAL:
			stack[top + 1] = base(stack, b, i.l);
			// generate new block mark
			stack[top + 2] = b;
			stack[top + 3] = pc;
			b = top + 1;
			pc = i.a;
			break;
		case INT:
			top += i.a;
			break;
		case JMP:
			pc = i.a;
			break;
		case JPC:
			if (stack[top] == 0)
				pc = i.a;
			top--;
			break;
		case JPX:  //添加为非0跳转指令
			if (stack[top] != 0)
				pc = i.a;
			top--;
			break;
		} // switch
	}
	while (pc);

	printf("End executing PL/0 program.\n");
} // interpret

//////////////////////////////////////////////////////////////////////
void main ()
{
	FILE* hbin;
	char s[80];
	int i;
	symset set, set1, set2;

	printf("Please input source file name: "); // get file name to be compiled
	scanf("%s", s);
	if ((infile = fopen(s, "r")) == NULL)
	{
		printf("File %s can't be opened.\n", s);
		exit(1);
	}

	phi = createset(SYM_NULL);
	relset = createset(SYM_EQU, SYM_NEQ, SYM_LES, SYM_LEQ, SYM_GTR, SYM_GEQ,SYM_NULL);//关系运算符号集
	// create begin symbol sets
	declbegsys = createset(SYM_CONST, SYM_VAR, SYM_PROCEDURE, SYM_NULL);
	statbegsys = createset(SYM_BEGIN, SYM_CALL, SYM_IF, SYM_WHILE,SYM_FOR,SYM_EXIT,SYM_BREAK, SYM_NULL);
	facbegsys = createset(SYM_IDENTIFIER, SYM_NUMBER, SYM_LPAREN,SYM_NOT,SYM_ODD, SYM_NULL);  //这里把单目运算符not,od添加到因子级别

	err = cc = cx = ll = 0; // initialize global variables
	ch = ' ';
	kk = MAXIDLEN;

	getsym();

	set1 = createset(SYM_PERIOD, SYM_NULL);
	set2 = uniteset(declbegsys, statbegsys);
	set = uniteset(set1, set2);
	block(set);
	destroyset(set1);
	destroyset(set2);
	destroyset(set);
	destroyset(phi);
	destroyset(relset);
	destroyset(declbegsys);
	destroyset(statbegsys);
	destroyset(facbegsys);

	if (sym != SYM_PERIOD)
		error(9); // '.' expected.
	if (err == 0)
	{
		hbin = fopen("hbin.txt", "w");
		for (i = 0; i < cx; i++)
			fwrite(&code[i], sizeof(instruction), 1, hbin);
		fclose(hbin);
	}
	if (err == 0)
		interpret();
	else
		printf("There are %d error(s) in PL/0 program.\n", err);
	listcode(0, cx);
} // main

//////////////////////////////////////////////////////////////////////
// eof pl0.c
