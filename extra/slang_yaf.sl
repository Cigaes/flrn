define int_of_hex_digit (c) {
    if ((c >= 48) and (c <= 58))
       return (c-48);
       else return (c-87);
}

define unquote_group_name (n)
{
    variable n0 = substr(n,5,-1);
    variable n1 = strtrans(n0,"_"," ");
    variable pos=1,len;
    variable c;
    while (string_match(n1,"\\(\\...\\)",pos))
    {
       (pos,len) = string_match_nth(1);
       c = int_of_hex_digit(int(substr(n1,pos+2,1))) *16 +
           int_of_hex_digit(int(substr(n1,pos+3,1)));
       (n1, ) = str_replace (n1,substr(n1,pos+1,3),char(c)); 
       pos = pos+2;
    }
    return n1;
}

define flags_group (a)
{
    variable i;
    i = get_flags_group(a);
    if (i & 1) 
       return "[D]";
    else 
       return "";
}

define Newsgroup_title_string (a)
{
    return strcat(flags_group(a),unquote_group_name(string(a)));
}
