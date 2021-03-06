/*
 * Alex Lapena
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>

#include "calutil.h" 

#define MAX 1024

/*Takes the line then the folded line
 * and removes the ending characters 
 * then adds them together into a single
 * string 
 * 
 * Returns a string which is to be stored
 * 
 * */
char * folding(char * buff, char *tail);

int main( int argc, const char* argv[] )
{
	FILE *fp;
	CalComp **pcomp = malloc(sizeof(CalComp *)*1);
	CalStatus status;
	
	fp = fopen("test.ics", "r");
	if(fp == NULL) 
	{
		perror("Error opening file");
		return(-1);
	}
	status = readCalFile(fp, pcomp);
	
	fclose(fp); 
	
	return(0);
}

CalStatus readCalFile( FILE *const ics, CalComp **const pcomp )
{
	CalStatus status;
	
	status = readCalLine(NULL, NULL);
	
	(*pcomp) = malloc(sizeof(CalComp)+sizeof(char**));
	
	(*pcomp)->name = NULL;
    (*pcomp)->nprops = 0; 
    (*pcomp)->prop = NULL;
    (*pcomp)->ncomps = 0;
    (*pcomp)->comp[0] = NULL; 
    
	
    status = readCalComp(ics, pcomp);
    freeCalComp(*pcomp);
    
    return(status);	
}

CalStatus readCalComp( FILE *const ics, CalComp **const pcomp ){
	CalStatus status;
	static int depth = 0;
	int properties = 0;
	static int lineNum = 0;
	
	CalProp *const prop = malloc(sizeof(CalProp));
	char **str = malloc(sizeof(char*));

	while(!feof(ics)){
		/*First line read in, checks if BEGIN:VCALENDAR*/
		if(prop->name == NULL){
			status = readCalLine(ics,str);
			status.code = parseCalProp(*str, prop);
			lineNum++;
			if(strcmp(prop->name, "BEGIN")==0 && strcmp(prop->value, "VCALENDAR")==0){
				depth++;
				(*pcomp)->name = malloc(sizeof(char)*strlen(prop->name)+1); 
				strcpy((*pcomp)->name,prop->value);
				printf("%s\n",(*pcomp)->name);
			}
			else{
				status.code = NOCAL;
				return(status);
			}
		}
		/*Other lines stored into components of the property*/
		else{
			status = readCalLine(ics,str);
			status.code = parseCalProp(*str, prop);
			lineNum++;
			if(strcmp(prop->name, "BEGIN") == 0 && depth <= 3){
				depth++;
				(*pcomp)->name = malloc(sizeof(char)*strlen(prop->name)+1); 
				strcpy((*pcomp)->name,prop->value);
				status = readCalComp(ics, pcomp);
				printf("%s\n",(*pcomp)->name);
			}
			else if(strcmp(prop->name, "END") == 0){
				if(strcmp((*pcomp)->name, prop->value)==0){
					depth--;
				}
				else if((*pcomp)->nprops == 0 && (*pcomp)->ncomps == 0){
					status.code = NODATA;
					return(status);
				}
				else{
					status.code = BEGEND;
					return(status);
				}
			}
			else if(depth > 3){
				status.code = SUBCOM;
				return(status);
			}
			/*If a component*/
			else{
				(*pcomp)->nprops++;
				(*pcomp)->ncomps = depth;
				(*pcomp)->prop = malloc(sizeof(CalProp)); 
				(*pcomp)->prop->name = malloc(sizeof(char)* strlen(prop->name)+1);
				(*pcomp)->prop->value = malloc(sizeof(char)* strlen(prop->value)+1);
				strcpy((*pcomp)->prop->name,prop->name);
				strcpy((*pcomp)->prop->value,prop->value);
				properties++;
				printf("Component: %s\n",(*pcomp)->prop->name);
			}
		}
	}
	
	status.linefrom = lineNum;
	status.lineto = lineNum+1;
	status.code = OK;
	
	return(status);
}

CalStatus readCalLine( FILE *const ics, char **const pbuff ){
	CalStatus status; 
	int i;
	char line[1024];
	char newS[1024];
	char clean[1024];
	char * folded;
	static char strB[1024];
	int check = 0;
	static int first = 0;
	static int lineNum;

	if(ics == NULL){
		status.linefrom = 0;
		status.lineto = 0;
		status.code = OK;
		return(status);
	}
	
	while(check == 0){	
		fgets(line,78,ics);
		/*Checks for whitespace (folding) then creates a string to be concated*/
		if(line[0] == ' ' || line[0] == '\t'){	
			for(i = 1; i <= strlen(line); i++){
				newS[i-1] = line[i];
				if(i == strlen(line)){
					newS[i] = '\0'; 
				}
			}
			/*removes ending characters from first line then concats them together*/
			folded = folding(strB, newS);
			strcpy(strB,folded);
		}
		/*Handling non-folded case*/
		else if((line[0] != ' ' || line[0] != '\t') && (first == 0)){	
			for(i = 0; i <= (strlen(line)-2); i++){
				clean[i] = line[i];
				if(i == strlen(line)-2){
					clean[i] = '\0';
				}
			}
			strcpy(strB,clean);
			first = 1;
		}
		else if((line[0] != ' ' || line[0] != '\t') && (first == 1)){	
			*pbuff = malloc(sizeof(char)*strlen(strB)+2);
			strcpy(*pbuff,strB);
			for(i = 0; i <= (strlen(line)-2); i++){
				clean[i] = line[i];
				if(i == strlen(line)-2){
					clean[i] = '\0';
				}
			}
			strcpy(strB,clean);
			check = 1;
		}
		//Check for blank lines still
		else{
			status.code = NOCRNL;
			status.linefrom = lineNum;
			status.lineto = lineNum+1;
			return(status);
		}
	}
	//free(folded);
	
	status.code = OK;
	status.linefrom = lineNum;
	status.lineto = lineNum+1;
	lineNum++;
	
	return(status);
}
CalError parseCalProp( char *const buff, CalProp *const prop ){
	int i = 0, j = 0;
	int quotesOn = 0, toValue = 0, toPName = 0, toPVal = 0, hasParam = 0, multPName = 0, nParams = 1; 
	char tempName[MAX];
	char tempValue[MAX];
	char tempPName[MAX];
	char tempPVal[MAX];
	
	prop->name = NULL;
	prop->value = NULL; 
    prop->nparams = 0;
    prop->param = NULL;
    prop->next = NULL; 
    
    prop->param = malloc(sizeof(CalParam)+sizeof(char**));
        
    if(strcmp(buff, "")==0){
		return(SYNTAX);
	}
	
	for(i = 0; i < strlen(buff); i++){
		//Checks for quotes
		if(buff[i] == '\"' && quotesOn == 0){
			quotesOn = 1;
		}
		else if(buff[i] == '\"' && quotesOn == 1){
			quotesOn = 0;
		}
		
		//Checks for delimiting characters
		if(buff[i] == ':' && quotesOn == 0 && toValue == 0){
			//If there is no Param, stores name
			if(hasParam == 0){
				prop->name = malloc(sizeof(char)*strlen(tempName)+1);
				strcpy(prop->name,tempName);
				printf("name: %s\n", prop->name);
			}
			//Storing first iteration of params if there are more to come
			if(toPVal == 1 && nParams == 1){
				prop->param->value[nParams-1] = malloc(sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			//Storing other iterations of params
			else if(toPVal == 1 && nParams > 1){
				prop->param->value[nParams-1] = realloc(prop->param->value[nParams-1], sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			toValue = 1;
			j = 0;
			//Send latest parameter section to memory
		}
		
		else if(buff[i] == ';' && quotesOn == 0 && toValue == 0){
			//Stores name if there is a param
			if(hasParam == 0){
				prop->name = malloc(sizeof(char)*strlen(tempName)+1);
				strcpy(prop->name,tempName);
				hasParam = 1;
				printf("name: %s\n", prop->name);
			}
			//Storing first iteration of params if there are more to come
			if(toPVal == 1 && nParams == 1){
				prop->param->value[nParams-1] = malloc(sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			//Storing other iterations of params
			else if(toPVal == 1 && nParams > 1){
				prop->param->value[nParams-1] = realloc(prop->param->value[nParams-1], sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			toPName = 1;
			toPVal = 0;
			j = 0;
		}
		
		else if(buff[i] == '=' && quotesOn == 0 && toValue == 0){
			//First parameter name stored in memory
			if(multPName == 0){
				prop->param->name = malloc(sizeof(char)*strlen(tempPName)+1);
				strcpy(prop->param->name, tempPName);
				tempPName[0] = '\0';
				multPName = 1;				
			}
			//If multiple parameters
			else if(multPName == 1){
				prop->param->name = realloc(prop->param->name, sizeof(char)*strlen(tempPName)+1);
				strcpy(prop->param->name, tempPName);
				tempPName[0] = '\0';
			}
			printf("pName: %s\n",prop->param->name);
			toPName = 0;
			toPVal = 1;
			j = 0;
		}
		
		else if(buff[i] == ',' && quotesOn == 0 && toValue == 0){
			//Storing first iteration of params if there are more to come
			if(toPVal == 1 && nParams == 1){
				prop->param->value[nParams-1] = malloc(sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			//Storing other iterations of params
			else if(toPVal == 1 && nParams > 1){
				prop->param->value[nParams-1] = realloc(prop->param->value[nParams-1], sizeof(char)*strlen(tempPVal)+1);
				strcpy(prop->param->value[nParams-1], tempPVal);
				tempPVal[0] = '\0';
				printf("pVal: %s\n", prop->param->value[nParams-1]);
				nParams++;
			}
			j = 0;
		}
		
		//Allocates characters into parsed values
		
		//Name Parser
		else if(toValue == 0 && toPName == 0 && toPVal == 0){
			if(isdigit(buff[i]) || isalpha(buff[i]) || buff[i] == '-'){
				tempName[i] = buff[i];
				tempName[i] = toupper(tempName[i]);
				tempName[i+1] = '\0';
			}
			else{
				return(SYNTAX);
			}
		}
		//Value parser
		else if(toValue == 1){
			tempValue[j] = buff[i];
			tempValue[j+1] = '\0';
			j++;
		}
		//Parameter Name parser
		else if(toValue == 0 && toPName == 1 && toPVal == 0){
			if(isdigit(buff[i]) || isalpha(buff[i]) || buff[i] == '-'){
				tempPName[j] = buff[i];
				tempPName[j] = toupper(tempPName[j]);
				tempPName[j+1] = '\0';
				j++;
			}
			else{
				return(SYNTAX);
			}
		}
		//parameter Value parser
		else if(toValue == 0 && toPName == 0 && toPVal == 1){
			tempPVal[j] = buff[i];
			tempPVal[j+1] = '\0';
			j++;
		}
		else{
			return(SYNTAX);
		}
	}
	prop->value = malloc(sizeof(char)*strlen(tempValue)+1);
	strcpy(prop->value, tempValue);
	printf("Value: %s\n", prop->value);
		
	return(OK);
}

void freeCalComp( CalComp *const comp ){
	
}

/*Takes the line then the folded line
 * and removes the ending characters 
 * then adds them together into a single
 * string 
 * Returns a string which is to be stored
 * 
 * */
char * folding(char * buff, char * tail){
	int i;
	char * clean = malloc(sizeof(char*));
	
	for(i = 0; i <= (strlen(buff)-2); i++){
		clean[i] = buff[i];
	}
	/*removes the trailing end of line characters
	 * from the tail string*/
	for(i = 0; i <= (strlen(tail)-2); i++){
		if(i == strlen(tail)-2){
			tail[i] = '\0';
		}
	}
	strcat(clean,tail);
	return(clean);
}
