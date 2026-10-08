void Solve(Matrix &A, Matrix &E)
{
    size_t n=A.m;

    // первый шаг ручками. ГЛАВНЫЙ ПО СТРОКЕ
    size_t ind;
    ind=A.MainElement_Row(0,0);
    E.SwapColumns(0,ind); A.SwapColumns(0,ind); //сначала делаем для E, т.к. E зависит от того, что в А, а А не зависит от того, что в E.
    E.MultColumn(0,1/A.mat[0][0]); A.MultColumn(0,1/A.mat[0][0]);
    for(size_t i=1; i<n; i++)
    {
        E.SubtractColumns(i,0,A.mat[0][i]); A.SubtractColumns(i,0,A.mat[0][i]);
    }
    for(size_t k=1; k<n; k++)
    {
        ind=A.MainElement_Row(k,k);
        E.SwapColumns(k,ind); A.SwapColumns(k,ind);
        E.MultColumn(k,1/A.mat[k][k]); A.MultColumn(k,1/A.mat[k][k]);
        for(size_t i=0; i<n; i++)
        {
            if(i!=k)
            {
                E.SubtractColumns(i,k,A.mat[k][i]); A.SubtractColumns(i,k,A.mat[k][i]); // if очень важен, иначе появится нулевая строка, но богачеву в книжке пофиг, его прога не работала бы
            }
        }
    } // A стала единичной, E стала A^(-1)





}
