// Версия 2
struct pthread_arg
{
    size_t	p_current; //номер этого потока
    size_t p_total; // кол-во потоков
    Matrix *A; // поток будет преобразовывать эту матрицу
    Matrix *E; // и эту матрицу. Указатели, потому что мои потоки должны именно менять матрицу!!!
};

void *thread_program(void *arg) // то, что делает поток
{
    struct pthread_arg	*parg=(struct pthread_arg*)arg; // приводим аргумент к нужному типу
    size_t id=parg->p_current;
    size_t n=parg->A->n;
    size_t total=parg->p_total;

    size_t ind;
    for(size_t k=0; k<n; k++)
    {
        if(parg->p_current==0) // начальfные действия пусть выполняет 0-ой поток
        {
            ind=parg->A->MainElement_Row(k,k);
            parg->E->SwapColumns(k,ind); parg->A->SwapColumns(k,ind);
            if(fabs(parg->A->mat[k][k])<EPS)
            {
                cout<<endl<<endl<<"DIVING BY 0!!!!!!!!!! STOOOOOP!!!!"<<endl<<endl;
                return 0;
            }
            parg->E->MultColumn(k,1/parg->A->mat[k][k]); parg->A->MultColumn(k,1/parg->A->mat[k][k]);
        }
        synchronize(parg->p_total);
        for(size_t i=id*n/total; i<(id+1)*n/total; i++)
        {
            if(i!=k)
            {
                parg->E->SubtractColumns(i,k,parg->A->mat[k][i]); parg->A->SubtractColumns(i,k,parg->A->mat[k][i]);
            }
        }
        synchronize(parg->p_total); // Все сделали свое дело на этой шаге, можно продолжить менять матрицу
    }

    return 0; // хз, зачем вообще что-то возвращать
}

